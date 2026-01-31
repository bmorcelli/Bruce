
#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <globals.h>

struct IRCode {
    IRCode(
        String protocol = "", String address = "", String command = "", String data = "", uint8_t bits = 32
    )
        : protocol(protocol), address(address), command(command), data(data), bits(bits) {}

    IRCode(IRCode *code) {
        name = String(code->name);
        type = String(code->type);
        protocol = String(code->protocol);
        address = String(code->address);
        command = String(code->command);
        frequency = code->frequency;
        bits = code->bits;
        // duty_cycle = code->duty_cycle;
        data = String(code->data);
        filepath = String(code->filepath);
    }

    String protocol = "";
    String address = "";
    String command = "";
    String data = "";
    uint8_t bits = 32;
    String name = "";
    String type = "";
    uint16_t frequency = 0;
    // float duty_cycle;
    String filepath = "";
};

enum class IREncodingType : uint8_t {
    PulseDistance,
    PulseWidth,
    Biphase,
};

struct IRProtocolInfo {
    const char *name = "";
    IREncodingType encoding = IREncodingType::PulseDistance;
    uint32_t carrier_hz = 38000;
    float duty_cycle = 0.33f;
    uint16_t header_mark = 0;
    uint16_t header_space = 0;
    uint16_t one_mark = 0;
    uint16_t one_space = 0;
    uint16_t zero_mark = 0;
    uint16_t zero_space = 0;
    uint16_t trailer_mark = 0;
    uint16_t trailer_space = 0;
    bool lsb_first = true;
    uint16_t unit = 0; // base time for biphase protocols
    uint8_t nbits = 0;
};

// Custom IR
void sendIRCommand(IRCode *code, bool hideDefaultUI = false);
void sendRawCommand(uint16_t frequency, String rawData, bool hideDefaultUI = false);
void sendNECCommand(String address, String command, bool hideDefaultUI = false);
void sendNECextCommand(String address, String command, bool hideDefaultUI = false);
void sendRC5Command(String address, String command, bool hideDefaultUI = false);
void sendRC6Command(String address, String command, bool hideDefaultUI = false);
void sendSamsungCommand(String address, String command, bool hideDefaultUI = false);
void sendSonyCommand(String address, String command, uint8_t nbits, bool hideDefaultUI = false);
void sendKaseikyoCommand(String address, String command, bool hideDefaultUI = false);
bool sendDecodedCommand(String protocol, String value, uint8_t bits = 32, bool hideDefaultUI = false);
void otherIRcodes();
bool txIrFile(FS *fs, String filepath, bool hideDefaultUI = false);
bool chooseCmdIrFile(FS *fs, String filepath);
