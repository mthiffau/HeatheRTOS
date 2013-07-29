#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "xassert.h"
#include "array_size.h"
#include "u_tid.h"
#include "static_assert.h"
#include "sensor.h"
#include "track_graph.h"
#include "switch_srv.h"
#include "track_srv.h"
#include "track_pt.h"

void
mkcircuit(track_graph_t track, struct track_route *route)
{
    struct track_path *p;
    int i;
    route->n_paths = 1;
    p = &route->paths[0];
    p->hops = 0;
    route->edges[p->hops++] = &track->calib_sensors[0]->edge[TRACK_EDGE_AHEAD];
    while (route->edges[p->hops - 1]->dest != track->calib_sensors[0]) {
        int edge_ix;
        track_node_t src = route->edges[p->hops - 1]->dest;
        if (src->type != TRACK_NODE_BRANCH) {
            route->edges[p->hops++] = &src->edge[TRACK_EDGE_AHEAD];
            continue;
        }
        for (i = 0; i < track->n_calib_switches; i++) {
            if (track->calib_switches[i] == src)
                break;
        }
        assert(i < track->n_calib_switches);
        if (track->calib_curved[i])
            edge_ix = TRACK_EDGE_CURVED;
        else
            edge_ix = TRACK_EDGE_STRAIGHT;
        route->edges[p->hops++] = &src->edge[edge_ix];
    }

    route->track    = track;
    p->track        = track;
    p->edges        = route->edges;
    p->start.edge   = p->edges[0];
    p->start.pos_um = 1000 * p->start.edge->len_mm - 1;
    p->end.edge     = p->edges[p->hops - 1];
    p->end.pos_um   = 0;

    for (i = 0; i < (int)ARRAY_SIZE(p->edge_ix); i++)
        p->edge_ix[i] = -1;

    p->len_mm = 0;
    for (i = 0; i < (int)p->hops; i++) {
        p->len_mm += p->edges[i]->len_mm;
        TRACK_EDGE_DATA(track, p->edges[i], p->edge_ix) = i;
    }
}
