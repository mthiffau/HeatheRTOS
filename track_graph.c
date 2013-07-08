#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "static_assert.h"
#include "sensor.h"
#include "track_graph.h"

#include "xstring.h"
#include "xassert.h"
#include "array_size.h"
#include "pqueue.h"

int track_node_edges[6] = { 0, 1, 2, 1, 1, 0 };

track_graph_t
track_byname(const char *name)
{
    int i;
    for (i = 0; i < TRACK_COUNT; i++) {
        if (!strcmp(all_tracks[i]->name, name))
            return all_tracks[i];
    }
    return NULL;
}

track_node_t
track_node_byname(track_graph_t track, const char *name)
{
    int i;
    for (i = 0; i < track->n_nodes; i++) {
        track_node_t node = &track->nodes[i];
        if (!strcmp(node->name, name))
            return node;
    }
    return NULL;
}
