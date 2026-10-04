#include "core/flapi.h"
#include <stdio.h>

void Testing(void) {
    FlTimer p = fl_profile_start();
    FlNetStatus s = fl_net_status();
    switch(s) {
        case FL_NET_OFFLINE: printf("Offline\n"); break;
        case FL_NET_ETHERNET: printf("Ethernet\n"); break;
        case FL_NET_WIFI: printf("Wi-Fi\n"); break;
    }
    fl_profile_end(p);
}



FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+V", Testing);
}
