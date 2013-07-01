#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "static_assert.h"
#include "sensor.h"
#include "track_graph.h"

#include "xdef.h"
#include "xassert.h"
#include "array_size.h"
#include "pqueue.h"

int track_node_edges[6] = { 0, 1, 2, 1, 1, 0 };

int
track_pathfind(
    const struct track_graph *track,
    const struct track_node  *src,
    const struct track_node  *dest,
    const struct track_node  *path[TRACK_NODES_MAX])
{
    struct node_info {
        bool                     visited;
        int                      distance; /* -1 means infinity */
        int                      hops;
        const struct track_node *parent;
    } node_info[TRACK_NODES_MAX];

    struct pqueue       border;
    struct pqueue_node  border_mem[TRACK_NODES_MAX];
    unsigned            i;
    int                 rc;

    /* Initialize auxiliary node data */
    pqueue_init(&border, ARRAY_SIZE(border_mem), border_mem);
    for (i = 0; i < ARRAY_SIZE(node_info); i++) {
        struct node_info *inf = &node_info[i];
        inf->visited  = false;
        inf->distance = -1;
        inf->hops     = 0;
        inf->parent   = NULL;
    }

    /* Start with just src at distance 0. */
    TRACK_NODE_DATA(track, src, node_info).distance = 0;
    rc = pqueue_add(&border, src - track->nodes, 0);
    assertv(rc, rc == 0);

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

        if (node == dest)
            break;

        cur_info = &TRACK_NODE_DATA(track, node, node_info);
        cur_info->visited = true;

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
                edge_dest_info->parent   = node;
                if (old_dist == -1)
                    rc = pqueue_add(&border, edge_dest_ix, new_dist);
                else
                    rc = pqueue_decreasekey(&border, edge_dest_ix, new_dist);
                assertv(rc, rc == 0);
            }
        }
    }

    /* Reconstruct path backwards */
    int path_hops = TRACK_NODE_DATA(track, dest, node_info).hops + 1;
    for (;;) {
        struct node_info *dest_info = &TRACK_NODE_DATA(track, dest, node_info);
        path[dest_info->hops] = dest;
        if (dest == src)
            break;
        dest = dest_info->parent;
    }
    return path_hops;
}
