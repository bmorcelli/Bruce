#pragma once

#include "protocol.h"

class protocol_linear : public c_rf_protocol {
public:
    protocol_linear() {
        transposition_table[0][0] = 500;
        transposition_table[0][1] = -1500;
        transposition_table[1][0] = 1500;
        transposition_table[1][1] = -500;
        pilot_period[0] = 0;
        pilot_period[1] = 0;
        stop_bit[0] = 1;
        stop_bit[1] = -21500;
    }
};
