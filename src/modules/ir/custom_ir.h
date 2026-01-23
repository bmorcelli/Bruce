
#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <globals.h>

#if !defined(BRUCE_DISABLE_IR)
#include <IRremoteESP8266.h>
#include <IRsend.h>
#endif

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

#if !defined(BRUCE_DISABLE_IR)
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
#else
inline void sendIRCommand(IRCode * /*code*/, bool /*hideDefaultUI*/ = false) {}
inline void sendRawCommand(uint16_t /*frequency*/, String /*rawData*/, bool /*hideDefaultUI*/ = false) {}
inline void sendNECCommand(String /*address*/, String /*command*/, bool /*hideDefaultUI*/ = false) {}
inline void sendNECextCommand(String /*address*/, String /*command*/, bool /*hideDefaultUI*/ = false) {}
inline void sendRC5Command(String /*address*/, String /*command*/, bool /*hideDefaultUI*/ = false) {}
inline void sendRC6Command(String /*address*/, String /*command*/, bool /*hideDefaultUI*/ = false) {}
inline void sendSamsungCommand(String /*address*/, String /*command*/, bool /*hideDefaultUI*/ = false) {}
inline void sendSonyCommand(String /*address*/, String /*command*/, uint8_t /*nbits*/, bool /*hideDefaultUI*/ = false) {}
inline void sendKaseikyoCommand(String /*address*/, String /*command*/, bool /*hideDefaultUI*/ = false) {}
inline bool sendDecodedCommand(String /*protocol*/, String /*value*/, uint8_t /*bits*/ = 32, bool /*hideDefaultUI*/ = false) { return false; }
inline void otherIRcodes() {}
inline bool txIrFile(FS * /*fs*/, String /*filepath*/, bool /*hideDefaultUI*/ = false) { return false; }
inline bool chooseCmdIrFile(FS * /*fs*/, String /*filepath*/) { return false; }
#endif
