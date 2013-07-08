#ifdef TRACK_PT_H
#error "double-included track_pt.h"
#endif

#define TRACK_PT_H

XBOOL_H;
TRACK_GRAPH_H;
SWITCH_SRV_H;

#define PATHFIND_DESTS_MAX      8

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

/* A path on the track between two points. */
struct track_path {
    track_graph_t   track;
    struct track_pt start, end; /* start on edges[0], end on edges[hops-1] */
    size_t          hops, len_mm;
    track_edge_t    edges[TRACK_NODES_MAX];
    size_t          n_branches;
    track_node_t    branches[TRACK_NODES_MAX];

    /* Index of each node in this path. Gives the edge OUT from that
     * node. Last node gets mapped to hops (1 past the end of edges).
     * Nodes outside of the path are mapped to -1. */
    int           node_ix[TRACK_NODES_MAX];
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

/* Advance a point along a given path. distance_um may be negative,
 * in which case the point is regressed backward along the path. */
void track_pt_advance_path(
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

/* Find a (good, directed) path from any src to any dest on
 * the given track. Only up to PATHFIND_DESTS_MAX destinations
 * may be provided. If a path is found, stores it in path_out
 * and returns 0. Otherwise, returns -1. */
int track_pathfind(
    track_graph_t          track,
    const struct track_pt *src_pts,
    unsigned               src_count,
    const struct track_pt *dests,
    unsigned               dest_count,
    struct track_path     *path_out);
