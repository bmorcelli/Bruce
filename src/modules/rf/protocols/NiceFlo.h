#pragma once

#include "protocol.h"

class protocol_nice_flo : public c_rf_protocol {
public:
    protocol_nice_flo() {
        transposition_table[0][0] = -700;
        transposition_table[0][1] = 1400;
        transposition_table[1][0] = -1400;
        transposition_table[1][1] = 700;
        pilot_period[0] = -25200;
        pilot_period[1] = 700;
        stop_bit[0] = 0;
        stop_bit[1] = 0;
    }
};
