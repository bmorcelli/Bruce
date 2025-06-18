#pragma once

#include "protocol.h"

class protocol_chamberlain : public c_rf_protocol {
public:
    protocol_chamberlain() {
        transposition_table[0][0] = -870;
        transposition_table[0][1] = 430;
        transposition_table[1][0] = -430;
        transposition_table[1][1] = 870;
        pilot_period[0] = 0;
        pilot_period[1] = 0;
        stop_bit[0] = -3000;
        stop_bit[1] = 1000;
    }
};
