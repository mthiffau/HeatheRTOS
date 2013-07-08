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

int
track_pathfind(
    track_graph_t          track,
    const struct track_pt *src_pts,
    unsigned               src_count,
    const track_node_t    *dests,
    unsigned               dest_count,
    struct track_path     *path_out)
{
    struct node_info {
        int                      distance; /* -1 means infinity */
        int                      hops;
        const struct track_edge *parent;
        bool                     visited;
        bool                     isdest;
    } node_info[TRACK_NODES_MAX];

    struct pqueue       border;
    struct pqueue_node  border_mem[TRACK_NODES_MAX];
    unsigned            i;
    int                 rc, path_hops;
    track_node_t        dest; /* holds the chosen destination */

    /* Initialize auxiliary node data */
    pqueue_init(&border, ARRAY_SIZE(border_mem), border_mem);
    for (i = 0; i < ARRAY_SIZE(node_info); i++) {
        struct node_info *inf = &node_info[i];
        inf->visited  = false;
        inf->isdest   = false;
        inf->distance = -1;
        inf->hops     = 0;
        inf->parent   = NULL;
    }

    /* Mark all destinations */
    for (i = 0; i < dest_count; i++)
        TRACK_NODE_DATA(track, dests[i], node_info).isdest = true;

    /* Start with destinations of source point edges at 1 hop and
     * at distances determined by the points. */
    for (i = 0; i < src_count; i++) {
        struct node_info *src_info;
        struct track_pt   src_pt = src_pts[i];
        src_info = &TRACK_NODE_DATA(track, src_pt.edge->dest, node_info);
        src_info->hops     = 1;
        src_info->distance = src_pt.pos_um / 1000;
        src_info->parent   = src_pt.edge;
        rc = pqueue_add(&border, src_pt.edge->dest - track->nodes, 0);
        assertv(rc, rc == 0);
    }

    /* DIJKSTRA */
    for (;;) {
        const struct track_node *node;
        struct pqueue_entry     *min;
        unsigned                 n_edges;
        struct node_info        *cur_info;

        min = pqueue_peekmin(&border);
        if (min == NULL)
            return -1;

        node = &track->nodes[min->val];
        pqueue_popmin(&border);

        cur_info = &TRACK_NODE_DATA(track, node, node_info);
        cur_info->visited = true;
        if (cur_info->isdest) {
            dest = node;
            break;
        }

        n_edges = (unsigned)track_node_edges[node->type];
        for (i = 0; i < n_edges; i++) {
            const struct track_edge *edge;
            struct node_info        *edge_dest_info;
            int                      edge_dest_ix;
            int                      old_dist, new_dist;
            edge           = &node->edge[i];
            edge_dest_ix   = edge->dest - track->nodes;
            edge_dest_info = &TRACK_NODE_DATA(track, edge->dest, node_info);
            if (edge_dest_info->visited)
                continue;

            old_dist = edge_dest_info->distance;
            new_dist = cur_info->distance + edge->len_mm;
            if (old_dist == -1 || old_dist > new_dist) {
                edge_dest_info->distance = new_dist;
                edge_dest_info->hops     = cur_info->hops + 1;
                edge_dest_info->parent   = edge;
                if (old_dist == -1)
                    rc = pqueue_add(&border, edge_dest_ix, new_dist);
                else
                    rc = pqueue_decreasekey(&border, edge_dest_ix, new_dist);
                assertv(rc, rc == 0);
            }
        }
    }

    /* Reconstruct path backwards */
    for (i = 0; i < ARRAY_SIZE(path_out->node_ix); i++)
        path_out->node_ix[i] = -1;

    path_hops           = TRACK_NODE_DATA(track, dest, node_info).hops;
    path_out->track     = track;
    path_out->hops      = path_hops;
    path_out->len_mm    = TRACK_NODE_DATA(track, dest, node_info).distance;
    TRACK_NODE_DATA(track, dest, path_out->node_ix) = path_hops;
    while (path_hops > 0) {
        struct node_info *dest_info = &TRACK_NODE_DATA(track, dest, node_info);
        assert(dest_info->hops == path_hops);
        path_out->edges[--path_hops] = dest_info->parent;
        dest = dest_info->parent->src;
        TRACK_NODE_DATA(track, dest, path_out->node_ix) = path_hops;
    }

    /* Make handy list of sensors */
    path_out->n_branches = 0;
    for (i = 0; i < path_out->hops; i++) {
        const struct track_node *src = path_out->edges[i]->src;
        if (src->type == TRACK_NODE_BRANCH)
            path_out->branches[path_out->n_branches++] = src;
    }

    dest = path_out->edges[path_out->hops - 1]->dest;
    if (dest->type == TRACK_NODE_BRANCH)
        path_out->branches[path_out->n_branches++] = dest;

    return 0;
}
