#include "core/flapi.h"
#include <stdio.h>

void Testing(void) {
    FlTimer p = fl_profile_start();
    unsigned long long rx = fl_net_rx_bytes_over(1.0);
    printf("%llu\n",rx);
    fl_profile_end(p);
}



FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
