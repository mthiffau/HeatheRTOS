#ifdef CIRCUIT_H
#error "double-included circuit.h"
#endif

#define CIRCUIT_H

TRACK_GRAPH_H;
TRACK_PT_H;

void mkcircuit(track_graph_t track, struct track_route *route);
