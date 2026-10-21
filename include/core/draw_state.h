#ifndef CORE_DRAW_STATE_H
#define CORE_DRAW_STATE_H

#include <stdbool.h>

typedef struct {
    int off_x;
    int off_y;
    unsigned long bg_override;
    bool transparent;

    bool measuring;
    int meas_x0;
    int meas_y0;
    int meas_x1;
    int meas_y1;
    int meas_origin_x;
    int meas_origin_y;
} FlDrawState;

FlDrawState* FlDrawState_Get(void);
void FlDrawState_BeginMeasure(void);
void FlDrawState_TrackMeasure(int x, int y, int w, int h);
void FlDrawState_EndMeasure(int* out_w, int* out_h);

#endif
