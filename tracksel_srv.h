#ifdef TRACKSEL_SRV_H
#error "double-included tracksel_srv.h"
#endif

#define TRACKSEL_SRV_H

TRACK_GRAPH_H;

/* Track selection server entry point. */
void trackselsrv_main(void);

/* Ask for the track. */
track_graph_t tracksel_ask(void);

/* Tell everybody what the track is. */
void tracksel_tell(track_graph_t track);
