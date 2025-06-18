#pragma once

#include "protocol.h"

class protocol_ansonic : public c_rf_protocol {
public:
    protocol_ansonic() {
        transposition_table[0][0] = -1111;
        transposition_table[0][1] = 555;
        transposition_table[1][0] = -555;
        transposition_table[1][1] = 1111;
        pilot_period[0] = -19425;
        pilot_period[1] = 555;
        stop_bit[0] = 0;
        stop_bit[1] = 0;
    }
};
