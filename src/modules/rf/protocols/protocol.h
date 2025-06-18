#ifndef PROTOCOL_H
#define PROTOCOL_H

class c_rf_protocol {
public:
    static const int TRANS_TABLE_SIZE = 2;
    static const int PULSE_PAIR_SIZE = 2;

    int transposition_table[TRANS_TABLE_SIZE][PULSE_PAIR_SIZE];
    int pilot_period[PULSE_PAIR_SIZE];
    int stop_bit[PULSE_PAIR_SIZE];

    c_rf_protocol() = default;
    virtual ~c_rf_protocol() = default;
};

#endif
