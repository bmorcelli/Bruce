#ifndef __RF_JAMMER_H__
#define __RF_JAMMER_H__

class RFJammer {
public:
    RFJammer(bool full = false);
    ~RFJammer();

    void setup();

private:
    int nTransmitterPin;
    bool sendRF = true;
    bool fullJammer = false;

    void display_banner();
    void run_full_jammer();
    void run_itmt_jammer();
    void send_optimized_pulse(uint32_t width);
    void send_random_pattern(uint32_t numPulses);
};

#endif
