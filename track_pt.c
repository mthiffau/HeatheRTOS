#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "static_assert.h"
#include "sensor.h"
#include "track_graph.h"
#include "u_tid.h"
#include "switch_srv.h"
#include "track_pt.h"

void
track_pt_reverse(struct track_pt *pt)
{
    pt->edge   = pt->edge->reverse;
    pt->pos_um = pt->edge->len_mm * 1000 - pt->pos_um;
}

bool
track_pt_advance(
    struct switchctx *switches,
    struct track_pt *pt,
    const struct track_node *landmark,
    int distance_um)
{
    bool past_landmark = false;
    pt->pos_um -= distance_um;
    while (pt->pos_um < 0) {
        const struct track_node *next_src = pt->edge->dest;
        int edge_ix;
        if (next_src == landmark)
            past_landmark = true;
        edge_ix = TRACK_EDGE_AHEAD;
        if (next_src->type == TRACK_NODE_BRANCH) {
            bool curved = switch_iscurved(switches, next_src->num);
            edge_ix = curved ? TRACK_EDGE_CURVED : TRACK_EDGE_STRAIGHT;
        }
        pt->edge    = &next_src->edge[edge_ix];
        pt->pos_um += 1000 * pt->edge->len_mm;
    }
    return past_landmark;
}

bool
track_pt_advance_path(
    struct track_path *path,
    struct track_pt *pt,
    const struct track_node *landmark,
    int distance_um)
{
    bool past_landmark = false;
    pt->pos_um -= distance_um;
    while (pt->pos_um < 0) {
        const struct track_node *next_src = pt->edge->dest;
        int edge_ix;
        if (next_src == landmark)
            past_landmark = true;
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
    return past_landmark;
}
