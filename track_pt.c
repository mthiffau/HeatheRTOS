#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "static_assert.h"
#include "sensor.h"
#include "track_graph.h"
#include "u_tid.h"
#include "switch_srv.h"
#include "track_pt.h"

#include "xassert.h"
#include "array_size.h"
#include "pqueue.h"

#define TRACK_ROUTESPEC_MAGIC   0x146ce017
#define PATHFIND_DESTS_MAX      2

static void track_pt_advance_path_rev(
    const struct track_path *path,
    struct track_pt *pt,
    int distance_um);

void
track_pt_reverse(struct track_pt *pt)
{
    pt->edge   = pt->edge->reverse;
    pt->pos_um = pt->edge->len_mm * 1000 - pt->pos_um;
}

void
track_pt_from_node(track_node_t node, struct track_pt *pt)
{
    switch (node->type) {
    case TRACK_NODE_SENSOR:
    case TRACK_NODE_BRANCH:
    case TRACK_NODE_EXIT:
        pt->edge   = node->reverse->edge[TRACK_EDGE_AHEAD].reverse;
        pt->pos_um = 0;
        break;
    case TRACK_NODE_ENTER:
    case TRACK_NODE_MERGE:
        pt->edge   = &node->edge[TRACK_EDGE_AHEAD];
        pt->pos_um = 1000 * pt->edge->len_mm - 1;
        break;
    default:
        panic("panic! bad track node type %d", node->type);
    }
}

void
track_pt_advance(
    struct switchctx *switches,
    struct track_pt *pt,
    int distance_um)
{
    pt->pos_um -= distance_um;
    while (pt->pos_um < 0) {
        track_node_t next_src;
        int edge_ix;
        next_src = pt->edge->dest;
        if (next_src->type == TRACK_NODE_EXIT) {
            pt->pos_um = 0;
            return;
        }
        edge_ix = TRACK_EDGE_AHEAD;
        if (next_src->type == TRACK_NODE_BRANCH) {
            bool curved = switch_iscurved(switches, next_src->num);
            edge_ix = curved ? TRACK_EDGE_CURVED : TRACK_EDGE_STRAIGHT;
        }
        pt->edge    = &next_src->edge[edge_ix];
        pt->pos_um += 1000 * pt->edge->len_mm;
    }
}

void
track_pt_advance_path(
    const struct track_path *path,
    struct track_pt *pt,
    int distance_um)
{
    if (distance_um < 0) {
        track_pt_advance_path_rev(path, pt, distance_um);
        return;
    }
    pt->pos_um -= distance_um;
    while (pt->pos_um < 0) {
        track_node_t next_src;
        int edge_ix;
        next_src = pt->edge->dest;
        if (next_src->type == TRACK_NODE_EXIT) {
            pt->pos_um = 0;
            return;
        }
        edge_ix = TRACK_EDGE_AHEAD;
        if (next_src->type == TRACK_NODE_BRANCH) {
            bool curved;
            unsigned j = TRACK_NODE_DATA(path->track, next_src, path->node_ix);
            if (j >= path->hops)
                curved = false;
            else
                curved = path->edges[j] == &next_src->edge[TRACK_EDGE_CURVED];
            edge_ix = curved ? TRACK_EDGE_CURVED : TRACK_EDGE_STRAIGHT;
        }
        pt->edge    = &next_src->edge[edge_ix];
        pt->pos_um += 1000 * pt->edge->len_mm;
    }
}

static void
track_pt_advance_path_rev(
    const struct track_path *path,
    struct track_pt *pt,
    int distance_um)
{
    pt->pos_um -= distance_um;
    while (pt->pos_um >= 1000 * pt->edge->len_mm) {
        track_node_t next_dest = pt->edge->src;
        int edge_ix;
        edge_ix = TRACK_NODE_DATA(path->track, next_dest, path->node_ix) - 1;
        assert(edge_ix >= 0);
        pt->pos_um -= 1000 * pt->edge->len_mm;
        pt->edge    = path->edges[edge_ix];
    }
}

int
track_pt_distance_path(
    const struct track_path *path,
    struct track_pt a,
    struct track_pt b)
{
    int dist_um = 0;
    int a_ix, b_ix;
    int sign = 1;

    a_ix = TRACK_NODE_DATA(path->track, a.edge->src, path->node_ix);
    b_ix = TRACK_NODE_DATA(path->track, b.edge->src, path->node_ix);
    assert(a_ix >= 0);
    assert(b_ix >= 0);

    if (a_ix > b_ix) {
        struct track_pt tmp;
        tmp  = a;
        a    = b;
        b    = tmp;
        sign = -1;
    }

    while (a.edge != b.edge) {
        track_node_t next_src;
        int edge_ix;
        dist_um += a.pos_um;
        next_src = a.edge->dest;
        if (next_src->type == TRACK_NODE_EXIT) {
            panic("panic! track_pt_distance_path() ran off end of %s",
                next_src->name);
        }
        edge_ix  = TRACK_NODE_DATA(path->track, next_src, path->node_ix);
        assert(edge_ix >= 0);
        assert((unsigned)edge_ix < path->hops);
        a.edge   = path->edges[edge_ix];
        a.pos_um = 1000 * a.edge->len_mm;
    }

    dist_um *= sign;
    dist_um += a.pos_um - b.pos_um;
    return dist_um;
}

#define ROUTEFIND_DEST_NONE     (-1)
#define ROUTEFIND_DEST_FORWARD  0
#define ROUTEFIND_DEST_REVERSE  1

struct routefind {
    const struct track_routespec *spec;

    struct node_info {
        int             distance; /* -1 means infinity */
        int             hops, bighops, totalhops;
        track_edge_t    parent;
        struct track_pt pathsrc;
        bool            visited;
    } node_info[TRACK_NODES_MAX];

    struct pqueue       border;
    struct pqueue_node  border_mem[TRACK_NODES_MAX + PATHFIND_DESTS_MAX];
    int                 edge_dest_sel[TRACK_EDGES_MAX]; /* ROUTEFIND_DEST_(*) */
};

static void
rfind_init(struct routefind *rf, const struct track_routespec *spec)
{
    unsigned i;
    int rc;

    /* Check validity of request. */
    assert(spec->magic                  == TRACK_ROUTESPEC_MAGIC);
    assert(spec->track                  != NULL);
    assert(spec->switches.switchsrv_tid >= 0);
    assert(spec->src_centre.edge        != NULL);
    assert(spec->src_centre.pos_um      >= 0);
    assert(spec->train_len_um           >= 0);
    assert(spec->err_um                 >= 0);
    /* init_rev_ok and rev_ok can't be invalid */
    assert(spec->dest.edge              != NULL);
    assert(spec->dest.pos_um            >= 0);

    /* Save the request */
    rf->spec = spec;

    /* Initialize auxiliary node data. */
    for (i = 0; i < ARRAY_SIZE(rf->node_info); i++) {
        struct node_info *inf = &rf->node_info[i];
        inf->distance = -1;
        inf->hops     = 0;
        inf->parent   = NULL;
        inf->visited  = false;
        inf->pathsrc.edge   = NULL;
        inf->pathsrc.pos_um = -1;
    }

    for (i = 0; i < ARRAY_SIZE(rf->edge_dest_sel); i++)
        rf->edge_dest_sel[i] = ROUTEFIND_DEST_NONE;

    /* Mark the destination in each direction. */
    TRACK_EDGE_DATA(spec->track, spec->dest.edge, rf->edge_dest_sel) =
        ROUTEFIND_DEST_FORWARD;
    TRACK_EDGE_DATA(spec->track, spec->dest.edge->reverse, rf->edge_dest_sel) =
        ROUTEFIND_DEST_REVERSE;

    /* Initialize border node priority queue. */
    pqueue_init(&rf->border, ARRAY_SIZE(rf->border_mem), rf->border_mem);

    /* Start with destinations of source point edges at 1 hop and
     * at distances determined by the points. */
    for (i = 0; i < (spec->init_rev_ok ? 2 : 1); i++) {
        struct node_info *src_info;
        struct track_pt   src_pt = spec->src_centre;
        if (i == 1)
            track_pt_reverse(&src_pt);
        src_info = &TRACK_NODE_DATA(
            spec->track,
            src_pt.edge->dest,
            rf->node_info);
        src_info->hops     = 1;
        src_info->bighops  = 0;
        src_info->totalhops= 1;
        src_info->distance = src_pt.pos_um / 1000;
        src_info->parent   = src_pt.edge;
        src_info->pathsrc  = src_pt;
        rc = pqueue_add(
            &rf->border,
            src_pt.edge->dest - spec->track->nodes,
            src_info->distance);
        assertv(rc, rc == 0);
    }
}

static int
rfind_dijkstra(struct routefind *rf, struct track_pt *final_dest_pt)
{
    unsigned i;
    int rc;
    for (;;) {
        const struct track_node *node;
        struct pqueue_entry     *min;
        unsigned                 n_edges;
        struct node_info        *cur_info;

        min = pqueue_peekmin(&rf->border);
        if (min == NULL)
            return -1;

        if (min->val >= (unsigned)rf->spec->track->n_nodes) {
            /* Not a node. It's a destination! */
            int which = min->val - rf->spec->track->n_nodes;
            *final_dest_pt = rf->spec->dest;
            if (which == ROUTEFIND_DEST_REVERSE)
                track_pt_reverse(final_dest_pt);
            else
                assert(which == ROUTEFIND_DEST_FORWARD);
            break;
        }

        node = &rf->spec->track->nodes[min->val];
        pqueue_popmin(&rf->border);

        cur_info = &TRACK_NODE_DATA(rf->spec->track, node, rf->node_info);
        cur_info->visited = true;
        n_edges = (unsigned)track_node_edges[node->type];
        for (i = 0; i < n_edges; i++) {
            const struct track_edge *edge;
            struct node_info        *edge_dest_info;
            int                      edge_dest_ix;
            int                      old_dist, new_dist;
            int                      dest_which;
            edge           = &node->edge[i];
            edge_dest_ix   = edge->dest - rf->spec->track->nodes;
            edge_dest_info = &TRACK_NODE_DATA(rf->spec->track, edge->dest, rf->node_info);

            dest_which = TRACK_EDGE_DATA(rf->spec->track, edge, rf->edge_dest_sel);
            if (dest_which >= 0) {
                struct track_pt dest_pt;
                unsigned dest_val;
                int      dest_dist;
                dest_val   = rf->spec->track->n_nodes + dest_which;
                dest_dist  = cur_info->distance;
                dest_dist += edge->len_mm;
                dest_pt    = rf->spec->dest;
                if (dest_which == ROUTEFIND_DEST_REVERSE)
                    track_pt_reverse(&dest_pt);
                dest_dist -= dest_pt.pos_um / 1000;
                rc = pqueue_add(&rf->border, dest_val, dest_dist);
                assertv(rc, rc == 0);
            }

            if (edge_dest_info->visited)
                continue;

            old_dist = edge_dest_info->distance;
            new_dist = cur_info->distance + edge->len_mm;
            if (old_dist == -1 || old_dist > new_dist) {
                edge_dest_info->distance = new_dist;
                edge_dest_info->hops     = cur_info->hops + 1;
                edge_dest_info->bighops  = cur_info->bighops;
                edge_dest_info->totalhops= cur_info->totalhops + 1;
                edge_dest_info->parent   = edge;
                edge_dest_info->pathsrc  = cur_info->pathsrc;
                if (old_dist == -1)
                    rc = pqueue_add(&rf->border, edge_dest_ix, new_dist);
                else
                    rc = pqueue_decreasekey(&rf->border, edge_dest_ix, new_dist);
                assertv(rc, rc == 0);
            }
        }
    }
    return 0;
}

static void
rfind_reconstruct(
    struct routefind *rf,
    struct track_pt dest_pt,
    struct track_route *route_out)
{
    unsigned i;
    /* Reconstruct path backwards */
    track_node_t dest  = dest_pt.edge->src;
    int bighops        = TRACK_NODE_DATA(rf->spec->track, dest, rf->node_info).bighops + 1;
    int totalhops      = TRACK_NODE_DATA(rf->spec->track, dest, rf->node_info).totalhops + 1;
    route_out->n_paths = bighops;
    while (--bighops >= 0) {
        int path_hops;
        struct track_path *path_out = &route_out->paths[bighops];
        for (i = 0; i < ARRAY_SIZE(path_out->node_ix); i++)
            path_out->node_ix[i] = -1;

        dest                = dest_pt.edge->src;
        path_out->end       = dest_pt;
        path_out->start     = TRACK_NODE_DATA(rf->spec->track, dest, rf->node_info).pathsrc;
        path_hops           = TRACK_NODE_DATA(rf->spec->track, dest, rf->node_info).hops + 1;
        path_out->track     = rf->spec->track;
        path_out->hops      = path_hops;
        path_out->len_mm    = 0;
        TRACK_NODE_DATA(rf->spec->track, dest_pt.edge->dest, path_out->node_ix) = path_hops;
        route_out->edges[--totalhops] = dest_pt.edge;
        path_hops--;
        path_out->len_mm += dest_pt.edge->len_mm - dest_pt.pos_um / 1000;
        TRACK_NODE_DATA(rf->spec->track, dest_pt.edge->src, path_out->node_ix) = path_hops;
        while (path_hops > 0) {
            struct node_info *dest_info = &TRACK_NODE_DATA(rf->spec->track, dest, rf->node_info);
            assert(dest_info->hops == path_hops);
            path_out->len_mm += dest_info->parent->len_mm;
            route_out->edges[--totalhops] = dest_info->parent;
            path_hops--;
            dest = dest_info->parent->src;
            TRACK_NODE_DATA(rf->spec->track, dest, path_out->node_ix) = path_hops;
        }
        path_out->edges = &route_out->edges[totalhops];
        path_out->len_mm -= path_out->start.edge->len_mm;
        path_out->len_mm += path_out->start.pos_um / 1000;
    }
}

int
track_routefind(
    const struct track_routespec *spec,
    struct track_route *route_out)
{
    struct routefind rf;
    int              rc;
    struct track_pt  dest_pt;
    rfind_init(&rf, spec);
    rc = rfind_dijkstra(&rf, &dest_pt);
    if (rc == 0)
        rfind_reconstruct(&rf, dest_pt, route_out);
    return rc;
}

void track_routespec_init(struct track_routespec *q)
{
    q->magic                  = TRACK_ROUTESPEC_MAGIC;
    q->track                  = NULL;
    q->switches.switchsrv_tid = -1;
    q->src_centre.edge        = NULL;
    q->src_centre.pos_um      = -1;
    q->err_um                 = -1;
    q->train_len_um           = -1;
    q->dest.edge              = NULL;
    q->dest.pos_um            = -1;
}
