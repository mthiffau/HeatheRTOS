#ifdef UI_SRV_H
#error "double-included ui_srv.h"
#endif

#define UI_SRV_H

CONFIG_H;
XINT_H;
U_TID_H;

/* Entry point for the UI server. */
void uisrv_main(void);

struct uictx {
    tid_t uisrv_tid;
};

/* Initialize a UI context (by finding the UI server task's TID) */
void uictx_init(struct uictx *ui);

/* Update the time displayed in the UI */
void ui_set_time(struct uictx *ui, int ticks);

/* Notify the UI of newly tripped sensors. */
void ui_sensors(struct uictx *ui, uint8_t new_sensors[SENSOR_BYTES]);
