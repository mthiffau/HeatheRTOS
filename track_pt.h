#ifdef TRACK_PT_H
#error "double-included track_pt.h"
#endif

#define TRACK_PT_H

XBOOL_H;
TRACK_GRAPH_H;
SWITCH_SRV_H;

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

/* Reverse the given point in place. */
void track_pt_reverse(struct track_pt *pt);

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
