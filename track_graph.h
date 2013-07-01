#ifdef TRACK_GRAPH_H
#error "double-included track_graph.h"
#endif

#define TRACK_GRAPH_H

XBOOL_H;
SENSOR_H;

/* Nodes on the track are a location paired with a direction. */
enum {
    TRACK_NODE_NONE   = 0,
    TRACK_NODE_SENSOR = 1, /* sensor (inherently directional) */
    TRACK_NODE_BRANCH = 2, /* turn-out in the branching direction */
    TRACK_NODE_MERGE  = 3, /* turn-out in the merging direction */
    TRACK_NODE_ENTER  = 4, /* track end-point, facing in */
    TRACK_NODE_EXIT   = 5, /* track end-point, facing out */
};

/* Each node has at most two directed edges leaving it.
 * The number of edges is strictly dependent on the node type.
 * Exits have no edges out. */

/* Sensors, merges, and entrances have a single edge. */
enum {
    TRACK_EDGE_AHEAD = 0
};

/* Branches have a straight edge and a curved edge. */
enum {
    TRACK_EDGE_STRAIGHT = 0,
    TRACK_EDGE_CURVED   = 1,
};

struct track_node;

struct track_edge {
    const struct track_node *src, *dest;
    const struct track_edge *reverse; /* same stretch of track, other direction */
    int                len_mm;
};

/* A node in the track graph. */
struct track_node {
    int                      type;    /* TRACK_NODE_(*) */
    const char              *name;
    int                      num;     /* sensor or switch number */
    const struct track_node *reverse; /* same location, other direction */
    struct track_edge        edge[2];
};

/* An entire track graph.
 * The graph must have exactly one node per sensor on the track,
 * and the node for sensor i must be at index i. */
struct track_graph {
    const struct track_node *nodes;
    int n_nodes, n_sensors;
};

/* Get the declarations for tracks that exist. */
#include "track/list.h"
#define TRACK_EDGES_MAX     (TRACK_NODES_MAX * 2)

/* For 'attaching' data to track nodes.
 * Use an array of size TRACK_NODES_MAX.
 * Macro returns an lvalue - good for set as well. */
#define TRACK_NODE_DATA(track, node, array) \
    ((array)[((node) - (track)->nodes)])

/* For 'attaching' data to track edges.
 * Use an array of size TRACK_EDGES_MAX.
 * Macro returns an lvalue - good for set as well. */
#define TRACK_EDGE_DATA(track, e, array) \
    ((array)[(((e)->src - (track)->nodes) << 1) | ((e) - (e)->src->edge)])

/* Find a (good, directed) path from src to dest on the given track.
 * If a path is found, stores it in path and returns its length (the
 * number of nodes in it). src and dest are both included in the path.
 * Otherwise, returns -1. */
int track_pathfind(
    const struct track_graph *track,
    const struct track_node  *src,
    const struct track_node  *dests,
    const struct track_node  *path[TRACK_NODES_MAX]);

/* Array specifying the number of edges for a given track node type. */
extern int track_node_edges[6];