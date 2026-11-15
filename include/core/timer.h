#ifndef CORE_TIMER_H
#define CORE_TIMER_H

#include <stdint.h>

#include "core/types.h"
#include "platform/platform.h"

#define TIMER_LOG_CAP 65536
#define TIMER_BUF_LINES 8192
#define TIMER_BUF_LINE 256

int64_t timer_now_ticks(void);
int64_t timer_tick_frequency(void);

#endif
