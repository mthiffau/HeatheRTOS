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
    const struct track_edge *edge;
    int                      pos_um;
};

/* Reverse the given point in place. */
void track_pt_reverse(struct track_pt *pt);

/* Advance a point along the current track state. Returns true if the
 * point passes landmark on its way. */
bool track_pt_advance(
    struct switchctx *switches,
    struct track_pt *pt,
    const struct track_node *landmark,
    int distance_um);

/* Advance a point along a given path. Returns true if the
 * point passes landmark on its way. */
bool track_pt_advance_path(
    struct track_path *path,
    struct track_pt *pt,
    const struct track_node *landmark,
    int distance_um);
