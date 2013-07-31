#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "static_assert.h"
#include "sensor.h"
#include "track_graph.h"
#include "u_tid.h"
#include "switch_srv.h"
#include "track_srv.h"
#include "track_pt.h"

#include "xassert.h"
#include "array_size.h"
#include "pqueue.h"

#define TRACK_ROUTESPEC_MAGIC           0x146ce017
#define PATHFIND_DESTS_MAX              2
#define PATHFIND_SOFTRES_PENALTY_MM     500

static void track_pt_advance_path_rev(
    struct switchctx *switches,
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
    track_pt_advance_trace(switches, pt, distance_um, NULL, NULL, 0);
}

void
track_pt_advance_trace(
    struct switchctx *switches,
    struct track_pt *pt,
    int distance_um,
    track_edge_t *edges,
    int *n_edges,
    int max_edges)
{
    int out_i = 0;
    assert(edges == NULL || max_edges >= 1);

    pt->pos_um -= distance_um;
    if (edges != NULL)
        edges[out_i++] = pt->edge;

    while (pt->pos_um < 0) {
        track_node_t next_src;
        int edge_ix;
        next_src = pt->edge->dest;
        if (next_src->type == TRACK_NODE_EXIT) {
            pt->pos_um = 0;
            break;
        }
        edge_ix = TRACK_EDGE_AHEAD;
        if (next_src->type == TRACK_NODE_BRANCH) {
            bool curved = switch_iscurved(switches, next_src->num);
            edge_ix = curved ? TRACK_EDGE_CURVED : TRACK_EDGE_STRAIGHT;
        }
        pt->edge    = &next_src->edge[edge_ix];
        pt->pos_um += 1000 * pt->edge->len_mm;

        if (edges != NULL) {
            assert(out_i < max_edges);
            edges[out_i++] = pt->edge;
        }
    }

    if (n_edges != NULL)
        *n_edges = out_i;
}

void
track_pt_advance_path(
    struct switchctx *switches,
    const struct track_path *path,
    struct track_pt *pt,
    int distance_um)
{
    if (distance_um < 0) {
        track_pt_advance_path_rev(switches, path, pt, distance_um);
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
        edge_ix = TRACK_EDGE_DATA(path->track, pt->edge, path->edge_ix);
        if (edge_ix >= 0 && (unsigned)edge_ix < path->hops - 1) {
            pt->edge = path->edges[edge_ix + 1];
        } else {
            edge_ix = TRACK_EDGE_AHEAD;
            if (next_src->type == TRACK_NODE_BRANCH) {
                bool curved;
                curved = switch_iscurved(switches, next_src->num);
                edge_ix = curved ? TRACK_EDGE_CURVED : TRACK_EDGE_STRAIGHT;
            }
            pt->edge = &next_src->edge[edge_ix];
        }
        pt->pos_um += 1000 * pt->edge->len_mm;
    }
}

static void
track_pt_advance_path_rev(
    struct switchctx *switches,
    const struct track_path *path,
    struct track_pt *pt,
    int distance_um)
{
    pt->pos_um -= distance_um;
    while (pt->pos_um >= 1000 * pt->edge->len_mm) {
        track_node_t next_dest, next_rdest;
        int edge_ix;
        next_dest  = pt->edge->src;
        next_rdest = next_dest->reverse;
        if (next_dest->type == TRACK_NODE_ENTER) {
            pt->pos_um = 1000 * pt->edge->len_mm;
            return;
        }
        pt->pos_um -= 1000 * pt->edge->len_mm;
        edge_ix = TRACK_EDGE_DATA(path->track, pt->edge, path->edge_ix);
        if (edge_ix >= 1) {
            pt->edge = path->edges[edge_ix - 1];
        } else {
            edge_ix = TRACK_EDGE_AHEAD;
            if (next_rdest->type == TRACK_NODE_BRANCH) {
                bool curved;
                curved = switch_iscurved(switches, next_dest->num);
                edge_ix = curved ? TRACK_EDGE_CURVED : TRACK_EDGE_STRAIGHT;
            }
            pt->edge = next_rdest->edge[edge_ix].reverse;
        }
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

    a_ix = TRACK_EDGE_DATA(path->track, a.edge, path->edge_ix);
    b_ix = TRACK_EDGE_DATA(path->track, b.edge, path->edge_ix);
    assert(a_ix >= 0);
    assert(b_ix >= 0);

    if (a_ix > b_ix || (a_ix == b_ix && a.pos_um < b.pos_um)) {
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
        edge_ix = TRACK_EDGE_DATA(path->track, a.edge, path->edge_ix);
        assert(edge_ix >= 0);
        assert((unsigned)edge_ix < path->hops - 1);
        a.edge   = path->edges[edge_ix + 1];
        a.pos_um = 1000 * a.edge->len_mm;
    }

    dist_um += a.pos_um - b.pos_um;
    dist_um *= sign;
    return dist_um;
}

#define ROUTEFIND_DEST_NONE     (-1)
#define ROUTEFIND_DEST_FORWARD  0
#define ROUTEFIND_DEST_REVERSE  1

struct rf_node_info {
    int             distance; /* -1 means infinity */
    int             hops, bighops, totalhops;
    track_edge_t    parent;
    track_edge_t    reverse_edges[16];
    int             n_reverse_edges;
    struct track_pt reverse_pt;
    struct track_pt pathsrc;
    bool            visited;
};

struct routefind {
    const struct track_routespec *spec;
    struct rf_node_info node_info[TRACK_NODES_MAX];
    struct rf_node_info dest_info[2];
    struct pqueue       border;
    struct pqueue_node  border_mem[TRACK_NODES_MAX + PATHFIND_DESTS_MAX];
    int8_t              edge_dest_sel[TRACK_EDGES_MAX]; /* ROUTEFIND_DEST_(*) */
};

static inline struct rf_node_info*
rf_node_info(struct routefind *rf, track_node_t node)
{
    return &TRACK_NODE_DATA(rf->spec->track, node, rf->node_info);
}

static inline int8_t*
rf_edge_dest(struct routefind *rf, track_edge_t edge)
{
    return &TRACK_EDGE_DATA(rf->spec->track, edge, rf->edge_dest_sel);
}

static inline bool
rf_dest_valid(struct routefind *rf, int dest)
{
    return dest == ROUTEFIND_DEST_FORWARD
        || (dest == ROUTEFIND_DEST_REVERSE && !rf->spec->dest_unidir);
}

static inline void
rf_checkdest(struct routefind *rf, int dest)
{
    assert(rf_dest_valid(rf, dest));
}

static inline size_t
rf_node2val(struct routefind *rf, track_node_t node)
{
    return node - rf->spec->track->nodes;
}

static inline size_t
rf_dest2val(struct routefind *rf, int dest)
{
    rf_checkdest(rf, dest);
    return rf->spec->track->n_nodes + dest;
}

static inline bool
rf_val_isnode(struct routefind *rf, size_t val)
{
    return val < (unsigned)rf->spec->track->n_nodes;
}

static inline track_node_t
rf_val2node(struct routefind *rf, size_t val)
{
    assert(rf_val_isnode(rf, val));
    return rf->spec->track->nodes + val;
}

static inline int
rf_val2dest(struct routefind *rf, size_t val)
{
    assert(!rf_val_isnode(rf, val));
    return val - rf->spec->track->n_nodes;
}

static inline struct track_pt
rf_getdest(struct routefind *rf, int dest)
{
    struct track_pt p;
    rf_checkdest(rf, dest);
    p = rf->spec->dest;
    if (dest == ROUTEFIND_DEST_REVERSE)
        track_pt_reverse(&p);
    return p;
}

static inline struct rf_node_info*
rf_dest_info(struct routefind *rf, int dest)
{
    rf_checkdest(rf, dest);
    return &rf->dest_info[dest];
}

static int
rf_dequeue(struct routefind *rf, size_t *val_out)
{
    struct pqueue_entry *entry;
    entry = pqueue_peekmin(&rf->border);
    if (entry == NULL)
        return -1;

    *val_out = entry->val;
    pqueue_popmin(&rf->border);
    return 0;
}

static void
rfind_init(struct routefind *rf, const struct track_routespec *spec)
{
    unsigned i;
    int rc;

    /* Check validity of request. */
    assert(spec->magic                   == TRACK_ROUTESPEC_MAGIC);
    assert(spec->track                   != NULL);
    assert(spec->switches                != NULL);
    assert(spec->switches->switchsrv_tid >= 0);
    assert(spec->res                     != NULL);
    assert(spec->res->tracksrv_tid       >= 0);
    assert(spec->train_id                >= 0);
    assert(spec->src_centre.edge         != NULL);
    assert(spec->src_centre.pos_um       >= 0);
    assert(spec->train_len_um            >= 0);
    assert(spec->err_um                  >= 0);
    /* init_rev_ok and rev_ok can't be invalid */
    assert(spec->rev_penalty_mm          >= 0);
    assert(spec->rev_slack_mm            >= 0);
    assert(spec->dest.edge               != NULL);
    assert(spec->dest.pos_um             >= 0);

    /* Save the request */
    rf->spec = spec;

    /* Initialize auxiliary node data. */
    for (i = 0; i < ARRAY_SIZE(rf->node_info); i++) {
        struct rf_node_info *inf = &rf->node_info[i];
        inf->distance = -1;
        inf->hops     = 0;
        inf->parent   = NULL;
        inf->visited  = false;
        inf->pathsrc.edge   = NULL;
        inf->pathsrc.pos_um = -1;
    }

    for (i = 0; i < ARRAY_SIZE(rf->dest_info); i++) {
        struct rf_node_info *inf = &rf->dest_info[i];
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
    *rf_edge_dest(rf, spec->dest.edge) = ROUTEFIND_DEST_FORWARD;
    if (!rf->spec->dest_unidir)
        *rf_edge_dest(rf, spec->dest.edge->reverse) = ROUTEFIND_DEST_REVERSE;

    /* Initialize border node priority queue. */
    pqueue_init(&rf->border, ARRAY_SIZE(rf->border_mem), rf->border_mem);

    /* Start with destinations of source point edges at 1 hop and
     * at distances determined by the points. */
    for (i = 0; i < (spec->init_rev_ok ? 2 : 1); i++) {
        struct rf_node_info *src_info, *src_dest_info;
        struct track_pt      src_pt = spec->src_centre;
        int                  src_dest;
        struct track_pt      src_dest_pt;
        if (i == 1)
            track_pt_reverse(&src_pt);
        src_info = rf_node_info(rf, src_pt.edge->dest);
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

        /* There may be a destination before the end of the edge. */
        src_dest = *rf_edge_dest(rf, src_pt.edge);
        if (src_dest == ROUTEFIND_DEST_NONE)
            continue; /* No destination on this edge */
        src_dest_pt = rf_getdest(rf, src_dest);
        assert(src_dest_pt.edge == src_pt.edge);
        if (src_dest_pt.pos_um > src_pt.pos_um)
            continue; /* Destination on this edge is behind src_pt. */
        src_dest_info = rf_dest_info(rf, src_dest);
        src_dest_info->hops      = 1;
        src_dest_info->bighops   = 0;
        src_dest_info->totalhops = 1;
        src_dest_info->distance  = (src_pt.pos_um - src_dest_pt.pos_um) / 1000;
        src_dest_info->parent    = src_pt.edge;
        src_dest_info->pathsrc   = src_pt;
        rc = pqueue_add(
            &rf->border,
            rf_dest2val(rf, src_dest),
            src_dest_info->distance);
    }
}

/* Returns -1 for an 'infinite' penalty (if the edge can't be used). */
static int
rfind_edge_penalty(struct routefind *rf, track_edge_t edge)
{
    struct reservation res;
    int owner;
    /* Reject edges that are owned by other trains. */
    track_query(rf->spec->res, edge, &res);
    if (res.disabled)
        return -1;
    owner = res.train_id;
    if (owner < 0 || owner == rf->spec->train_id)
        return 0;
    if (res.state == TRACK_SOFTRESERVED || res.state == TRACK_SOFTBLOCKED)
        return PATHFIND_SOFTRES_PENALTY_MM;
    return -1; /* (hard) owned by another train */
}

static void
rfind_consider_edge(struct routefind *rf, track_edge_t edge)
{
    track_node_t         src, dest;
    struct rf_node_info *src_info, *dest_info;
    int                  edge_penalty;
    int                  old_dist, new_dist;
    int                  dest_which;
    int                  rc;

    src       = edge->src;
    dest      = edge->dest;
    src_info  = rf_node_info(rf, src);
    dest_info = rf_node_info(rf, dest);

    /* Reject the 'wrong' branch of a turnout
     * if it's too close to starting position. */
    if (src->type == TRACK_NODE_BRANCH) {
        int threshold_mm = rf->spec->train_len_um / 2000;
        threshold_mm    += rf->spec->err_um / 1000;
        if (src_info->distance < threshold_mm) {
            bool curved = edge == &src->edge[TRACK_EDGE_CURVED];
            if (curved != switch_iscurved(rf->spec->switches, src->num))
                return; /* reject this edge */
        }
    }

    /* Reject edges that are owned by other trains. */
    edge_penalty = rfind_edge_penalty(rf, edge);
    if (edge_penalty < 0)
        return;

    /* Check for end destinations on the edge. */
    dest_which = *rf_edge_dest(rf, edge);
    if (rf_dest_valid(rf, dest_which)) {
        struct track_pt      dest_pt;
        int                  dest_dist;
        struct rf_node_info *end_info;
        size_t               dest_val;
        dest_dist  = src_info->distance;
        dest_dist += edge->len_mm;
        dest_dist += edge_penalty;
        dest_pt    = rf_getdest(rf, dest_which);
        dest_dist -= dest_pt.pos_um / 1000;
        end_info   = rf_dest_info(rf, dest_which);
        old_dist = end_info->distance;
        if (old_dist == -1 || old_dist > dest_dist) {
            end_info->distance  = dest_dist;
            end_info->hops      = src_info->hops + 1;
            end_info->bighops   = src_info->bighops;
            end_info->totalhops = src_info->totalhops + 1;
            end_info->parent    = edge;
            end_info->pathsrc   = src_info->pathsrc;
            dest_val            = rf_dest2val(rf, dest_which);
            if (old_dist == -1)
                rc = pqueue_add(&rf->border, dest_val, dest_dist);
            else
                rc = pqueue_decreasekey(&rf->border, dest_val, dest_dist);
            assertv(rc, rc == 0);
        }
    }

    if (dest_info->visited)
        return; /* We've already visited the destination of this edge. Ignore */

    old_dist = dest_info->distance;
    new_dist = src_info->distance + edge->len_mm + edge_penalty;
    if (old_dist == -1 || old_dist > new_dist) {
        size_t dest_val     = rf_node2val(rf, dest);
        dest_info->distance = new_dist;
        dest_info->hops     = src_info->hops + 1;
        dest_info->bighops  = src_info->bighops;
        dest_info->totalhops= src_info->totalhops + 1;
        dest_info->parent   = edge;
        dest_info->pathsrc  = src_info->pathsrc;
        if (old_dist == -1)
            rc = pqueue_add(&rf->border, dest_val, new_dist);
        else
            rc = pqueue_decreasekey(&rf->border, dest_val, new_dist);
        assertv(rc, rc == 0);
    }
}

static void
rfind_consider_reverse(struct routefind *rf, track_node_t merge)
{
    struct track_pt      reverse_pt;
    track_node_t         branch;
    track_edge_t         overshoot_edges[16];
    int                  n_edges;
    int                  overshoot_um;
    struct rf_node_info *merge_info, *branch_info;
    int                  old_dist, new_dist;
    int                  i, rc;

    if (!rf->spec->rev_ok || merge->type != TRACK_NODE_MERGE)
        return;

    branch = merge->reverse;
    assert(branch->type == TRACK_NODE_BRANCH);

    merge_info  = rf_node_info(rf, merge);
    branch_info = rf_node_info(rf, branch);

    overshoot_um  = rf->spec->train_len_um / 2;
    overshoot_um += 1000 * rf->spec->rev_slack_mm;

    old_dist  = branch_info->distance;
    new_dist  = merge_info->distance;
    new_dist += 2 * overshoot_um / 1000;
    new_dist += rf->spec->rev_penalty_mm;
    if (old_dist == -1 || old_dist > new_dist) {
        track_pt_from_node(merge, &reverse_pt);
        track_pt_advance_trace(
            rf->spec->switches,
            &reverse_pt,
            overshoot_um,
            overshoot_edges,
            &n_edges,
            ARRAY_SIZE(overshoot_edges));

        for (i = 0; i < n_edges; i++) {
            int edge_penalty = rfind_edge_penalty(rf, overshoot_edges[i]);
            if (edge_penalty < 0)
                return; /* necessary edges unavailable */
            new_dist += edge_penalty;
            edge_penalty = rfind_edge_penalty(rf, overshoot_edges[i]->reverse);
            if (edge_penalty < 0)
                return;
            new_dist += edge_penalty;
        }

        if (old_dist != -1 && old_dist <= new_dist)
            return; /* no longer beneficial after checking penalties */

        /* Update branch info. */
        size_t br_val           = rf_node2val(rf, branch);
        branch_info->distance   = new_dist;
        branch_info->hops       = n_edges;
        branch_info->bighops    = merge_info->bighops + 1;
        branch_info->totalhops  = merge_info->totalhops + 2 * n_edges;
        branch_info->parent     = NULL;
        branch_info->reverse_pt = reverse_pt;
        branch_info->pathsrc    = reverse_pt;

        track_pt_reverse(&branch_info->pathsrc);
        branch_info->n_reverse_edges = n_edges;
        for (i = 0; i < n_edges; i++) {
            int j = n_edges - 1 - i;
            branch_info->reverse_edges[j] = overshoot_edges[i]->reverse;
        }

        if (old_dist == -1)
            rc = pqueue_add(&rf->border, br_val, new_dist);
        else
            rc = pqueue_decreasekey(&rf->border, br_val, new_dist);
        assertv(rc, rc == 0);
    }
}

static int
rfind_visit(struct routefind *rf, struct track_pt *final_dest_pt)
{
    unsigned                 i;
    int                      rc;
    size_t                   next_val;
    const struct track_node *node;
    unsigned                 n_edges;

    rc = rf_dequeue(rf, &next_val);
    if (rc == -1)
        return -1;

    if (!rf_val_isnode(rf, next_val)) {
        /* Not a node. It's a destination! */
        *final_dest_pt = rf_getdest(rf, rf_val2dest(rf, next_val));
        return 0;
    }

    node = rf_val2node(rf, next_val);
    rf_node_info(rf, node)->visited = true;
    n_edges = (unsigned)track_node_edges[node->type];
    for (i = 0; i < n_edges; i++)
        rfind_consider_edge(rf, &node->edge[i]);

    rfind_consider_reverse(rf, node);
    return 1;
}

static int
rfind_dijkstra(struct routefind *rf, struct track_pt *final_dest_pt)
{
    int rc;
    for (;;) {
        rc = rfind_visit(rf, final_dest_pt);
        if (rc == -1)
            return -1;
        else if (rc == 0)
            break;
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
    int dest_which;
    /* Reconstruct path backwards */
    track_node_t dest  = dest_pt.edge->src;
    dest_which         = ROUTEFIND_DEST_FORWARD;
    if (dest_pt.edge != rf_getdest(rf, dest_which).edge) {
        dest_which = ROUTEFIND_DEST_REVERSE;
        assert(dest_pt.edge == rf_getdest(rf, dest_which).edge);
    }
    int bighops        = rf_dest_info(rf, dest_which)->bighops + 1;
    int totalhops      = rf_dest_info(rf, dest_which)->totalhops;
    int path_hops      = rf_dest_info(rf, dest_which)->hops;
    route_out->n_paths = bighops;
    while (--bighops >= 0) {
        struct track_path *path_out = &route_out->paths[bighops];

        for (i = 0; i < ARRAY_SIZE(path_out->edge_ix); i++)
            path_out->edge_ix[i] = -1;

        path_out->end       = dest_pt;
        if (bighops == rf_dest_info(rf, dest_which)->bighops)
            path_out->start = rf_dest_info(rf, dest_which)->pathsrc;
        else
            path_out->start = rf_node_info(rf, dest)->pathsrc;
        path_out->track     = rf->spec->track;
        path_out->hops      = path_hops;
        path_out->len_mm    = -dest_pt.pos_um / 1000;
        route_out->edges[--totalhops] = dest_pt.edge;
        path_hops--;
        if (bighops != route_out->n_paths - 1) {
            track_node_t branch;
            struct rf_node_info *branch_info;
            assert(dest->type == TRACK_NODE_MERGE);
            branch = dest->reverse;
            branch_info = rf_node_info(rf, branch);
            assert(branch_info->n_reverse_edges >= 0);
            for (i = 1; i < (unsigned)branch_info->n_reverse_edges; i++) {
                route_out->edges[--totalhops] =
                    branch_info->reverse_edges[i]->reverse;
                path_hops--;
            }
        }
        while (path_hops > 0) {
            struct rf_node_info *dest_info = rf_node_info(rf, dest);
            assert(dest_info->hops == path_hops);
            if (dest_info->parent == NULL)
                break;
            route_out->edges[--totalhops] = dest_info->parent;
            path_hops--;
            dest = dest_info->parent->src;
        }
        assert((bighops > 0) == (path_hops > 0));
        if (bighops > 0) {
            assert(dest->type == TRACK_NODE_BRANCH);
            struct rf_node_info *branch_info = rf_node_info(rf, dest);
            struct rf_node_info *merge_info;
            while (path_hops > 0) {
                /* Reversal edges! */
                assert(branch_info->parent == NULL);
                route_out->edges[--totalhops] =
                    branch_info->reverse_edges[--path_hops];
            }
            dest_pt = branch_info->reverse_pt;
            assert(dest->type == TRACK_NODE_BRANCH);
            dest = dest->reverse;
            merge_info = rf_node_info(rf, dest);
            path_hops = merge_info->hops + branch_info->n_reverse_edges;
        }

        path_out->edges = &route_out->edges[totalhops];
        for (i = 0; i < path_out->hops; i++) {
            track_edge_t edge = path_out->edges[i];
            path_out->len_mm += edge->len_mm;
            TRACK_EDGE_DATA(rf->spec->track, edge, path_out->edge_ix) = i;
        }

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
    /* Values marked DEFAULT are default values, since no invalid
     * value can be assigned to them. */
    q->magic                  = TRACK_ROUTESPEC_MAGIC;
    q->track                  = NULL;
    q->switches               = NULL;
    q->res                    = NULL;
    q->train_id               = -1;
    q->src_centre.edge        = NULL;
    q->src_centre.pos_um      = -1;
    q->err_um                 = -1;
    q->init_rev_ok            = true;  /* DEFAULT */
    q->rev_ok                 = true;  /* DEFAULT */
    q->rev_penalty_mm         = -1;
    q->rev_slack_mm           = -1;
    q->train_len_um           = -1;
    q->dest.edge              = NULL;
    q->dest.pos_um            = -1;
    q->dest_unidir            = false; /* DEFAULT */
}
