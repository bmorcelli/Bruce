#pragma once

#include "protocol.h"

class protocol_came : public c_rf_protocol {
public:
    protocol_came() {
        transposition_table[0][0] = -320;
        transposition_table[0][1] = 640;
        transposition_table[1][0] = -640;
        transposition_table[1][1] = 320;
        pilot_period[0] = -11520;
        pilot_period[1] = 320;
        stop_bit[0] = 0;
        stop_bit[1] = 0;
    }
};
