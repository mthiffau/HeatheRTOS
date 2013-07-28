#ifdef TRACK_PT_H
#error "double-included track_pt.h"
#endif

#define TRACK_PT_H

XBOOL_H;
TRACK_GRAPH_H;
SWITCH_SRV_H;
TRACK_SRV_H;
STATIC_ASSERT_H;

#define ROUTE_PATHS_MAX         4

/* A 'directed point' on the track, i.e. a point along a directed edge.
 * The point is represented by the directed edge and a distance remaining
 * to the destination node of that edge.
 *
 * Canonically, the distance remaining should be less than the length of
 * the edge.  */
struct track_pt {
    track_edge_t edge;
    int          pos_um;
};

/* A path on the track between two points.
 *
 * In a path, the destination of each edge is the source of the next edge. */
struct track_path {
    track_graph_t   track;
    struct track_pt start, end; /* start on edges[0], end on edges[hops-1] */
    size_t          hops, len_mm;
    track_edge_t   *edges;

    /* Index of each edge in this path.
     * Edges outside of the path are mapped to -1. */
    int16_t         edge_ix[TRACK_EDGES_MAX];
};

STATIC_ASSERT(track_path_node_ix_size, TRACK_NODES_MAX <= (1 << 15));

/* A route on the track between two points.
 *
 * A route consists of multiple paths. Each path's end point is the starting
 * point of the next path. */
struct track_route {
    track_graph_t     track;
    struct track_path paths[ROUTE_PATHS_MAX];
    int               n_paths;
    track_edge_t      edges[TRACK_NODES_MAX];
};

/* Reverse the given point in place. */
void track_pt_reverse(struct track_pt *pt);

/* Construct a point 'on' (or 'close to on') a node. */
void track_pt_from_node(track_node_t node, struct track_pt *pt);

/* Advance a point along the current track state. distance_um must
 * not be negative */
void track_pt_advance(
    struct switchctx *switches,
    struct track_pt *pt,
    int distance_um);

/* Advance a point along the current track state, keeping a record of
 * the edges over which it passes. distance_um must not be negative */
void
track_pt_advance_trace(
    struct switchctx *switches,
    struct track_pt *pt,
    int distance_um,
    track_edge_t *edges,
    int *n_edges,
    int max_edges);

/* Advance a point along a given path. distance_um may be negative,
 * in which case the point is regressed backward along the path. */
void track_pt_advance_path(
    struct switchctx *switches,
    const struct track_path *path,
    struct track_pt *pt,
    int distance_um);

/* Calculate the distance between two points along a given path,
 * in micrometers. Point a and point b must both occur on the path.
 * This gives the signed distance from a to b, which is negative if
 * b is behind a. */
int track_pt_distance_path(
    const struct track_path *path,
    struct track_pt a,
    struct track_pt b);

/* Find a (good) route for a train on the given track.
 * If a route is found, stores it in route_out
 * and returns 0. Otherwise, returns -1. */
struct track_routespec;
int track_routefind(
    const struct track_routespec *q,
    struct track_route *route_out);

struct track_routespec {
    int               magic;        /* Should be TRACK_ROUTESPEC_MAGIC (in .c) */

    track_graph_t     track;        /* What track? */
    struct switchctx *switches;     /* Switch state handle */
    struct trackctx  *res;          /* Reservation handle */

    int               train_id;     /* Which train? */
    struct track_pt   src_centre;   /* Initial position of train. */
    int               train_len_um; /* Length of the train */
    int               err_um;       /* Initial error bound (non-negative). */
    bool              init_rev_ok;  /* Is it okay to reverse at start? */
    bool              rev_ok;       /* Is it okay to reverse en route? */
    int               rev_penalty_mm;/* Distance penalty for reversals. */
    int               rev_slack_mm; /* Distance by which to overshoot merge. */

    struct track_pt   dest;         /* Destination for centre of train */
    bool              dest_unidir;  /* Force direction of arrival. */
};

void track_routespec_init(struct track_routespec *q);
