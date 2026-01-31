#include "custom_ir.h"
#include "TV-B-Gone.h" // for checkIrTxPin()
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/sd_functions.h"
#include "core/settings.h"
#include "ir_utils.h"
#include <driver/rmt_tx.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <vector>

uint32_t swap32(uint32_t value) {
    return ((value & 0x000000FF) << 24) | ((value & 0x0000FF00) << 8) | ((value & 0x00FF0000) >> 8) |
           ((value & 0xFF000000) >> 24);
}

static uint64_t reverseBits(uint64_t value, uint8_t nbits) {
    uint64_t out = 0;
    for (uint8_t i = 0; i < nbits; i++) {
        out = (out << 1) | (value & 0x1);
        value >>= 1;
    }
    return out;
}

struct LevelDuration {
    bool level = false;
    uint16_t duration = 0;
};

static rmt_channel_handle_t ir_tx_chan = NULL;
static rmt_encoder_handle_t ir_tx_encoder = NULL;
static uint32_t ir_tx_freq_hz = 0;
static float ir_tx_duty = -1.0f;
static bool ir_tx_invert = false;

static void ensure_ir_tx(uint32_t carrier_hz, float duty_cycle, bool invert) {
    if (ir_tx_chan == NULL) {
        rmt_tx_channel_config_t tx_cfg = {};
        tx_cfg.gpio_num = gpio_num_t(bruceConfigPins.irTx);
        tx_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
        tx_cfg.resolution_hz = 1000000; // 1 tick = 1us
        tx_cfg.mem_block_symbols = 64;
        tx_cfg.trans_queue_depth = 1;
        ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_cfg, &ir_tx_chan));
        ESP_ERROR_CHECK(rmt_enable(ir_tx_chan));
    }

    if (ir_tx_encoder == NULL) {
        rmt_copy_encoder_config_t encoder_cfg = {};
        ESP_ERROR_CHECK(rmt_new_copy_encoder(&encoder_cfg, &ir_tx_encoder));
    }

    if (carrier_hz != ir_tx_freq_hz || duty_cycle != ir_tx_duty || invert != ir_tx_invert) {
        rmt_carrier_config_t carrier_cfg = {};
        carrier_cfg.frequency_hz = carrier_hz;
        carrier_cfg.duty_cycle = duty_cycle;
        carrier_cfg.flags.polarity_active_low = invert;
        ESP_ERROR_CHECK(rmt_apply_carrier(ir_tx_chan, &carrier_cfg));
        ir_tx_freq_hz = carrier_hz;
        ir_tx_duty = duty_cycle;
        ir_tx_invert = invert;
    }
}

static void appendLevel(std::vector<LevelDuration> &levels, bool level, uint32_t duration_us) {
    while (duration_us > 0) {
        uint16_t chunk = duration_us > 32767 ? 32767 : static_cast<uint16_t>(duration_us);
        levels.push_back({level, chunk});
        duration_us -= chunk;
    }
}

static std::vector<uint16_t> levelsToDurations(const std::vector<LevelDuration> &levels) {
    std::vector<uint16_t> durations;
    bool started = false;
    for (const auto &entry : levels) {
        if (!started) {
            if (!entry.level) continue; // skip leading spaces
            started = true;
        }

        if (durations.empty()) {
            durations.push_back(entry.duration);
            continue;
        }

        bool is_mark = entry.level;
        bool last_is_mark = (durations.size() % 2 == 1);
        if (is_mark == last_is_mark) {
            uint32_t merged = durations.back() + entry.duration;
            if (merged > 0xFFFF) merged = 0xFFFF;
            durations.back() = static_cast<uint16_t>(merged);
        } else {
            durations.push_back(entry.duration);
        }
    }

    if (durations.size() % 2 != 0) durations.push_back(1); // pad trailing space
    return durations;
}

static bool rmtSendDurations(
    const std::vector<uint16_t> &durations, uint32_t carrier_hz, float duty_cycle, bool invert = false
) {
    if (durations.empty()) return false;

    ensure_ir_tx(carrier_hz, duty_cycle, invert);

    std::vector<LevelDuration> levels;
    bool lvl = true;
    for (size_t i = 0; i < durations.size(); i++) {
        appendLevel(levels, lvl, durations[i]);
        lvl = !lvl;
    }

    if (levels.size() % 2 != 0) levels.push_back({false, 1});

    std::vector<rmt_symbol_word_t> symbols;
    symbols.reserve((levels.size() + 1) / 2);
    for (size_t i = 0; i < levels.size(); i += 2) {
        rmt_symbol_word_t sym = {};
        sym.level0 = levels[i].level;
        sym.duration0 = levels[i].duration;
        sym.level1 = levels[i + 1].level;
        sym.duration1 = levels[i + 1].duration;
        symbols.push_back(sym);
    }

    rmt_transmit_config_t tx_trans_cfg = {};
    tx_trans_cfg.loop_count = 0;
    ESP_ERROR_CHECK(rmt_transmit(
        ir_tx_chan,
        ir_tx_encoder,
        symbols.data(),
        symbols.size() * sizeof(rmt_symbol_word_t),
        &tx_trans_cfg
    ));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(ir_tx_chan, portMAX_DELAY));

    return true;
}

static uint32_t parseHexString(String value) {
    value.replace(" ", "");
    if (value.isEmpty()) return 0;
    return strtoul(value.c_str(), nullptr, 16);
}

static std::vector<uint16_t> buildPulseDistanceTimings(
    uint64_t data, uint8_t nbits, const IRProtocolInfo &spec
) {
    std::vector<uint16_t> out;
    if (spec.header_mark) {
        out.push_back(spec.header_mark);
        out.push_back(spec.header_space);
    }

    for (uint8_t i = 0; i < nbits; i++) {
        uint8_t shift = spec.lsb_first ? i : (nbits - 1 - i);
        bool bit = (data >> shift) & 0x1;
        uint16_t mark = spec.one_mark ? spec.one_mark : spec.zero_mark;
        out.push_back(mark);
        out.push_back(bit ? spec.one_space : spec.zero_space);
    }

    if (spec.trailer_mark) {
        out.push_back(spec.trailer_mark);
        if (spec.trailer_space) out.push_back(spec.trailer_space);
    }

    if (out.size() % 2 != 0) out.push_back(1);
    return out;
}

static std::vector<uint16_t> buildPulseWidthTimings(
    uint64_t data, uint8_t nbits, const IRProtocolInfo &spec
) {
    std::vector<uint16_t> out;
    if (spec.header_mark) {
        out.push_back(spec.header_mark);
        out.push_back(spec.header_space);
    }

    for (uint8_t i = 0; i < nbits; i++) {
        uint8_t shift = spec.lsb_first ? i : (nbits - 1 - i);
        bool bit = (data >> shift) & 0x1;
        out.push_back(bit ? spec.one_mark : spec.zero_mark);
        out.push_back(spec.zero_space);
    }

    if (spec.trailer_mark) {
        out.push_back(spec.trailer_mark);
        if (spec.trailer_space) out.push_back(spec.trailer_space);
    }

    if (out.size() % 2 != 0) out.push_back(1);
    return out;
}

static void appendManchester(std::vector<LevelDuration> &levels, bool bit, uint16_t unit) {
    if (bit) {
        appendLevel(levels, true, unit);
        appendLevel(levels, false, unit);
    } else {
        appendLevel(levels, false, unit);
        appendLevel(levels, true, unit);
    }
}

static std::vector<uint16_t> buildBiphaseTimings(
    const std::vector<bool> &bits, uint16_t unit, bool double_toggle = false, int toggle_index = -1,
    uint16_t header_mark = 0, uint16_t header_space = 0
) {
    std::vector<LevelDuration> levels;
    if (header_mark) {
        appendLevel(levels, true, header_mark);
        appendLevel(levels, false, header_space);
    }

    for (size_t i = 0; i < bits.size(); i++) {
        uint16_t bit_unit = unit;
        if (double_toggle && static_cast<int>(i) == toggle_index) bit_unit = unit * 2;
        appendManchester(levels, bits[i], bit_unit);
    }

    return levelsToDurations(levels);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Custom IR

static std::vector<IRCode *> codes;

void resetCodesArray() {
    for (auto code : codes) { delete code; }
    codes.clear();
}

static std::vector<IRCode *> recent_ircodes;

void addToRecentCodes(IRCode *ircode) {
    // copy ircode -> recent_ircodes
    // if code exist in recent codes do not save it
    for (auto recent_ircode : recent_ircodes) {
        if (recent_ircode->filepath == ircode->filepath) { return; }
    }

    IRCode *ircode_copy = new IRCode(ircode);
    recent_ircodes.insert(recent_ircodes.begin(), ircode_copy);

    if (recent_ircodes.size() > 16) { // cycle
        delete recent_ircodes.back();
        recent_ircodes.pop_back();
    }
}

void selectRecentIrMenu() {
    // show menu with filenames
    checkIrTxPin();
    options = {};
    bool exit = false;
    IRCode *selected_code = NULL;
    for (auto recent_ircode : recent_ircodes) {
        if (recent_ircode->filepath == "") continue; // not inited
        // else
        options.push_back({recent_ircode->filepath.c_str(), [recent_ircode, &selected_code]() {
                               selected_code = recent_ircode;
                           }});
    }
    options.push_back({"Main Menu", [&]() { exit = true; }});

    int idx = 0;
    while (1) {
        idx = loopOptions(options, idx);
        if (selected_code != NULL) {
            sendIRCommand(selected_code);
            selected_code = NULL;
        }
        if (check(EscPress) || exit) break;
    }
    options.clear();

    return;
}

bool txIrFile(FS *fs, String filepath, bool hideDefaultUI) {
    // SPAM all codes of the file

    int total_codes = 0;
    String line;

    File databaseFile = fs->open(filepath, FILE_READ);

    setup_ir_pin(bruceConfigPins.irTx, OUTPUT);
    // digitalWrite(bruceConfigPins.irTx, LED_ON);

    if (!databaseFile) {
        Serial.println("Failed to open database file.");
        displayError("Fail to open file");
        delay(2000);
        return false;
    }
    Serial.println("Opened database file.");

    bool endingEarly = false;
    int codes_sent = 0;
    uint16_t frequency = 0;
    String rawData = "";
    String protocol = "";
    String address = "";
    String command = "";
    String value = "";
    uint8_t bits = 32;

    databaseFile.seek(0); // comes back to first position

    // count the number of codes to replay
    while (databaseFile.available()) {
        line = databaseFile.readStringUntil('\n');
        if (line.startsWith("type:")) total_codes++;
    }

    Serial.printf("\nStarted SPAM all codes with: %d codes", total_codes);
    // comes back to first position, beggining of the file
    databaseFile.seek(0);
    while (databaseFile.available()) {
        if (!hideDefaultUI) { progressHandler(codes_sent, total_codes); }
        line = databaseFile.readStringUntil('\n');
        if (line.endsWith("\r")) line.remove(line.length() - 1);

        if (line.startsWith("type:")) {
            codes_sent++;
            String type = line.substring(5);
            type.trim();
            Serial.println("Type: " + type);
            if (type == "raw") {
                Serial.println("RAW code");
                while (databaseFile.available()) {
                    line = databaseFile.readStringUntil('\n');
                    if (line.endsWith("\r")) line.remove(line.length() - 1);

                    if (line.startsWith("frequency:")) {
                        line = line.substring(10);
                        line.trim();
                        frequency = line.toInt();
                        Serial.printf("Frequency: %d\n", frequency);
                    } else if (line.startsWith("data:")) {
                        rawData = line.substring(5);
                        rawData.trim();
                        Serial.println("RawData: " + rawData);
                    } else if ((frequency != 0 && rawData != "") || line.startsWith("#")) {
                        IRCode code;
                        code.type = "raw";
                        code.data = rawData;
                        code.frequency = frequency;
                        sendIRCommand(&code, hideDefaultUI);

                        rawData = "";
                        frequency = 0;
                        type = "";
                        line = "";
                        break;
                    }
                }
            } else if (type == "parsed") {
                Serial.println("PARSED");
                while (databaseFile.available()) {
                    line = databaseFile.readStringUntil('\n');
                    if (line.endsWith("\r")) line.remove(line.length() - 1);

                    if (line.startsWith("protocol:")) {
                        protocol = line.substring(9);
                        protocol.trim();
                        Serial.println("Protocol: " + protocol);
                    } else if (line.startsWith("address:")) {
                        address = line.substring(8);
                        address.trim();
                        Serial.println("Address: " + address);
                    } else if (line.startsWith("command:")) {
                        command = line.substring(8);
                        command.trim();
                        Serial.println("Command: " + command);
                    } else if (line.startsWith("value:") || line.startsWith("state:")) {
                        value = line.substring(6);
                        value.trim();
                        Serial.println("Value: " + value);
                    } else if (line.startsWith("bits:")) {
                        bits = line.substring(strlen("bits:")).toInt();
                        Serial.println("bits: " + bits);
                    } else if (line.indexOf("#") != -1) { // TODO: also detect EOF
                        IRCode code(protocol, address, command, value, bits);
                        sendIRCommand(&code, hideDefaultUI);

                        protocol = "";
                        address = "";
                        command = "";
                        value = "";
                        bits = 32;
                        type = "";
                        line = "";
                        break;
                    }
                }
            }
        }
        // if user is pushing (holding down) TRIGGER button, stop transmission early
        if (check(SelPress)) // Pause TV-B-Gone
        {
            while (check(SelPress)) yield();
            if (!hideDefaultUI) { displayTextLine("Paused"); }

            while (!check(SelPress)) { // If Presses Select again, continues
                if (check(EscPress)) {
                    endingEarly = true;
                    break;
                }
            }
            while (check(SelPress)) { yield(); }
            if (endingEarly) break; // Cancels  custom IR Spam
            if (!hideDefaultUI) { displayTextLine("Running, Wait"); }
        }
    } // end while file has lines to process
    databaseFile.close();
    Serial.println("closed");
    Serial.println("EXTRA finished");

    resetCodesArray();
    digitalWrite(bruceConfigPins.irTx, LED_OFF);
    return true;
}

void otherIRcodes() {
    checkIrTxPin();
    resetCodesArray();
    String filepath;
    FS *fs = NULL;

    returnToMenu = true; // make sure menu is redrawn when quitting in any point

    options = {
        {"Recent",   selectRecentIrMenu       },
        {"LittleFS", [&]() { fs = &LittleFS; }},
        {"Menu",     yield                    },
    };
    if (setupSdCard()) options.insert(options.begin(), {"SD Card", [&]() { fs = &SD; }});

    loopOptions(options);

    if (fs == NULL) { // recent or menu was selected
        return;
        // no need to proceed, go back
    }

    // select a file to tx
    if (!(*fs).exists("/BruceIR")) (*fs).mkdir("/BruceIR");
    filepath = loopSD(*fs, true, "IR", "/BruceIR");
    if (filepath == "") return; //  cancelled

    // select mode
    bool exit = false;
    bool mode_cmd = true;
    options = {
        {"Choose cmd", [&]() { mode_cmd = true; } },
        {"Spam all",   [&]() { mode_cmd = false; }},
        {"Menu",       [&]() { exit = true; }     },
    };

    loopOptions(options);

    if (exit == true) return;

    if (mode_cmd == false) {
        // Spam all selected
        txIrFile(fs, filepath);
        return;
    }

    // else continue and call chooseCmdIrFile
    chooseCmdIrFile(fs, filepath);
} // end of otherIRcodes

// IR commands

void sendIRCommand(IRCode *code, bool hideDefaultUI) {
    setup_ir_pin(bruceConfigPins.irTx, OUTPUT);
    // https://developer.flipper.net/flipperzero/doxygen/infrared_file_format.html
    if (code->type.equalsIgnoreCase("raw")) sendRawCommand(code->frequency, code->data, hideDefaultUI);
    else if (code->protocol.equalsIgnoreCase("NEC"))
        sendNECCommand(code->address, code->command, hideDefaultUI);
    else if (code->protocol.equalsIgnoreCase("NECext"))
        sendNECextCommand(code->address, code->command, hideDefaultUI);
    else if (code->protocol.equalsIgnoreCase("RC5") || code->protocol.equalsIgnoreCase("RC5X"))
        sendRC5Command(code->address, code->command, hideDefaultUI);
    else if (code->protocol.equalsIgnoreCase("RC6"))
        sendRC6Command(code->address, code->command, hideDefaultUI);
    else if (code->protocol.equalsIgnoreCase("Samsung32"))
        sendSamsungCommand(code->address, code->command, hideDefaultUI);
    else if (code->protocol.equalsIgnoreCase("SIRC"))
        sendSonyCommand(code->address, code->command, 12, hideDefaultUI);
    else if (code->protocol.equalsIgnoreCase("SIRC15"))
        sendSonyCommand(code->address, code->command, 15, hideDefaultUI);
    else if (code->protocol.equalsIgnoreCase("SIRC20"))
        sendSonyCommand(code->address, code->command, 20, hideDefaultUI);
    else if (code->protocol.equalsIgnoreCase("Kaseikyo"))
        sendKaseikyoCommand(code->address, code->command, hideDefaultUI);
    else sendDecodedCommand(code->protocol, code->data, code->bits, hideDefaultUI);
}

void sendNECCommand(String address, String command, bool hideDefaultUI) {
    const IRProtocolInfo spec = {
        "NEC", IREncodingType::PulseDistance, 38000, 0.33f, 9000, 4500, 560, 1690, 560, 560, 560, 0, true, 0, 32
    };
    if (!hideDefaultUI) { displayTextLine("Sending.."); }
    uint16_t addressValue = strtoul(address.substring(0, 2).c_str(), nullptr, 16);
    uint16_t commandValue = strtoul(command.substring(0, 2).c_str(), nullptr, 16);
    uint32_t data = (addressValue & 0xFF) | ((~addressValue & 0xFF) << 8) |
                    ((commandValue & 0xFF) << 16) | ((~commandValue & 0xFF) << 24);
    std::vector<uint16_t> timings = buildPulseDistanceTimings(data, spec.nbits, spec);
    rmtSendDurations(timings, spec.carrier_hz, spec.duty_cycle);

    if (bruceConfigPins.irTxRepeats > 0) {
        for (uint8_t i = 1; i <= bruceConfigPins.irTxRepeats; i++) {
            rmtSendDurations(timings, spec.carrier_hz, spec.duty_cycle);
        }
    }

    Serial.println(
        "Sent NEC Command" + (bruceConfigPins.irTxRepeats > 0
                                  ? " (1 initial + " + String(bruceConfigPins.irTxRepeats) + " repeats)"
                                  : "")
    );

    digitalWrite(bruceConfigPins.irTx, LED_OFF);
}

void sendNECextCommand(String address, String command, bool hideDefaultUI) {
    const IRProtocolInfo spec = {
        "NECext",
        IREncodingType::PulseDistance,
        38000,
        0.33f,
        9000,
        4500,
        560,
        1690,
        560,
        560,
        560,
        0,
        true,
        0,
        32};
    if (!hideDefaultUI) { displayTextLine("Sending.."); }

    int first_zero_byte_pos = address.indexOf("00", 2);
    if (first_zero_byte_pos != -1) address = address.substring(0, first_zero_byte_pos);
    first_zero_byte_pos = command.indexOf("00", 2);
    if (first_zero_byte_pos != -1) command = command.substring(0, first_zero_byte_pos);

    address.replace(" ", "");
    command.replace(" ", "");

    uint16_t addressValue = strtoul(address.c_str(), nullptr, 16);
    uint16_t commandValue = strtoul(command.c_str(), nullptr, 16);

    // Invert Endianness
    uint16_t newAddress = (addressValue >> 8) | (addressValue << 8);
    uint16_t newCommand = (commandValue >> 8) | (commandValue << 8);

    // NEC protocol bit order is LSB first
    uint16_t lsbAddress = reverseBits(newAddress, 16);
    uint16_t lsbCommand = reverseBits(newCommand, 16);

    uint32_t data = ((uint32_t)lsbAddress << 16) | lsbCommand;
    std::vector<uint16_t> timings = buildPulseDistanceTimings(data, spec.nbits, spec);
    rmtSendDurations(timings, spec.carrier_hz, spec.duty_cycle);

    if (bruceConfigPins.irTxRepeats > 0) {
        for (uint8_t i = 1; i <= bruceConfigPins.irTxRepeats; i++) {
            rmtSendDurations(timings, spec.carrier_hz, spec.duty_cycle);
        }
    }

    Serial.println(
        "Sent NECext Command" + (bruceConfigPins.irTxRepeats > 0
                                     ? " (1 initial + " + String(bruceConfigPins.irTxRepeats) + " repeats)"
                                     : "")
    );
    digitalWrite(bruceConfigPins.irTx, LED_OFF);
}

void sendRC5Command(String address, String command, bool hideDefaultUI) {
    const IRProtocolInfo spec = {"RC5", IREncodingType::Biphase, 36000, 0.33f, 0, 0, 0, 0, 0, 0, 0, 0, false, 889, 14};
    if (!hideDefaultUI) { displayTextLine("Sending.."); }
    uint8_t addressValue = strtoul(address.substring(0, 2).c_str(), nullptr, 16);
    uint8_t commandValue = strtoul(command.substring(0, 2).c_str(), nullptr, 16);

    bool field = commandValue <= 0x3F;
    uint8_t rc5_command = commandValue & 0x3F;

    std::vector<bool> bits;
    bits.push_back(true);
    bits.push_back(true);
    bits.push_back(false); // toggle bit (static)
    bits.push_back(field);
    for (int i = 4; i >= 0; i--) bits.push_back((addressValue >> i) & 0x1);
    for (int i = 5; i >= 0; i--) bits.push_back((rc5_command >> i) & 0x1);

    std::vector<uint16_t> timings = buildBiphaseTimings(bits, spec.unit);
    rmtSendDurations(timings, spec.carrier_hz, spec.duty_cycle);

    if (bruceConfigPins.irTxRepeats > 0) {
        for (uint8_t i = 1; i <= bruceConfigPins.irTxRepeats; i++) {
            rmtSendDurations(timings, spec.carrier_hz, spec.duty_cycle);
        }
    }
    Serial.println(
        "Sent RC5 Command" + (bruceConfigPins.irTxRepeats > 0
                                  ? " (1 initial + " + String(bruceConfigPins.irTxRepeats) + " repeats)"
                                  : "")
    );
    digitalWrite(bruceConfigPins.irTx, LED_OFF);
}

void sendRC6Command(String address, String command, bool hideDefaultUI) {
    const IRProtocolInfo spec = {
        "RC6",
        IREncodingType::Biphase,
        36000,
        0.33f,
        2666,
        889,
        0,
        0,
        0,
        0,
        0,
        0,
        false,
        444,
        20};
    if (!hideDefaultUI) { displayTextLine("Sending.."); }
    address.replace(" ", "");
    command.replace(" ", "");
    uint32_t addressValue = strtoul(address.substring(0, 2).c_str(), nullptr, 16);
    uint32_t commandValue = strtoul(command.substring(0, 2).c_str(), nullptr, 16);

    std::vector<bool> bits;
    // RC6-0: mode bits 000, toggle bit, address (8), command (8)
    bits.push_back(false);
    bits.push_back(false);
    bits.push_back(false);
    bits.push_back(false); // toggle bit (static)
    for (int i = 7; i >= 0; i--) bits.push_back((addressValue >> i) & 0x1);
    for (int i = 7; i >= 0; i--) bits.push_back((commandValue >> i) & 0x1);

    std::vector<uint16_t> timings =
        buildBiphaseTimings(bits, spec.unit, true, 3, spec.header_mark, spec.header_space);
    rmtSendDurations(timings, spec.carrier_hz, spec.duty_cycle);

    if (bruceConfigPins.irTxRepeats > 0) {
        for (uint8_t i = 1; i <= bruceConfigPins.irTxRepeats; i++) {
            rmtSendDurations(timings, spec.carrier_hz, spec.duty_cycle);
        }
    }

    Serial.println(
        "Sent RC6 Command" + (bruceConfigPins.irTxRepeats > 0
                                  ? " (1 initial + " + String(bruceConfigPins.irTxRepeats) + " repeats)"
                                  : "")
    );
    digitalWrite(bruceConfigPins.irTx, LED_OFF);
}

void sendSamsungCommand(String address, String command, bool hideDefaultUI) {
    const IRProtocolInfo spec = {
        "Samsung32", IREncodingType::PulseDistance, 38000, 0.33f, 4500, 4500, 560, 1690, 560, 560, 560, 0, true, 0, 32
    };
    if (!hideDefaultUI) { displayTextLine("Sending.."); }
    String address_clean = address;
    String command_clean = command;
    address_clean.replace(" ", "");
    command_clean.replace(" ", "");

    uint32_t addressValue = parseHexString(address);
    uint32_t commandValue = parseHexString(command);

    uint16_t address16 =
        (address_clean.length() > 2) ? (addressValue & 0xFFFF) : ((addressValue & 0xFF) << 8) | (addressValue & 0xFF);
    uint16_t command16 =
        (command_clean.length() > 2) ? (commandValue & 0xFFFF) : ((commandValue & 0xFF) << 8) | (commandValue & 0xFF);

    uint32_t data = (uint32_t)address16 | ((uint32_t)command16 << 16);
    std::vector<uint16_t> timings = buildPulseDistanceTimings(data, spec.nbits, spec);
    rmtSendDurations(timings, spec.carrier_hz, spec.duty_cycle);

    if (bruceConfigPins.irTxRepeats > 0) {
        for (uint8_t i = 1; i <= bruceConfigPins.irTxRepeats; i++) {
            rmtSendDurations(timings, spec.carrier_hz, spec.duty_cycle);
        }
    }

    Serial.println(
        "Sent Samsung Command" + (bruceConfigPins.irTxRepeats > 0
                                      ? " (1 initial + " + String(bruceConfigPins.irTxRepeats) + " repeats)"
                                      : "")
    );
    digitalWrite(bruceConfigPins.irTx, LED_OFF);
}

void sendSonyCommand(String address, String command, uint8_t nbits, bool hideDefaultUI) {
    const IRProtocolInfo spec = {
        "SIRC", IREncodingType::PulseWidth, 40000, 0.33f, 2400, 600, 1200, 0, 600, 600, 0, 0, true, 0, nbits
    };
    if (!hideDefaultUI) { displayTextLine("Sending.."); }

    address.replace(" ", "");
    command.replace(" ", "");

    uint32_t addressValue = strtoul(address.c_str(), nullptr, 16);
    uint32_t commandValue = strtoul(command.c_str(), nullptr, 16);

    uint16_t swappedAddr = static_cast<uint16_t>(swap32(addressValue));
    uint8_t swappedCmd = static_cast<uint8_t>(swap32(commandValue));

    uint32_t data;

    if (nbits == 12) {
        // SIRC (12 bits)
        data = ((swappedAddr & 0x1F) << 7) | (swappedCmd & 0x7F);
    } else if (nbits == 15) {
        // SIRC15 (15 bits)
        data = ((swappedAddr & 0xFF) << 7) | (swappedCmd & 0x7F);
    } else if (nbits == 20) {
        // SIRC20 (20 bits)
        data = ((swappedAddr & 0x1FFF) << 7) | (swappedCmd & 0x7F);
    } else {
        Serial.println("Invalid Sony (SIRC) protocol bit size.");
        return;
    }

    // SIRC protocol bit order is LSB First
    data = reverseBits(data, nbits);

    std::vector<uint16_t> timings = buildPulseWidthTimings(data, nbits, spec);
    rmtSendDurations(timings, spec.carrier_hz, spec.duty_cycle);

    if (bruceConfigPins.irTxRepeats > 0) {
        for (uint8_t i = 1; i <= bruceConfigPins.irTxRepeats; i++) {
            rmtSendDurations(timings, spec.carrier_hz, spec.duty_cycle);
        }
    }

    Serial.println(
        "Sent Sony Command" + (bruceConfigPins.irTxRepeats > 0
                                   ? " (1 initial + " + String(bruceConfigPins.irTxRepeats) + " repeats)"
                                   : "")
    );
    digitalWrite(bruceConfigPins.irTx, LED_OFF);
}

void sendKaseikyoCommand(String address, String command, bool hideDefaultUI) {
    const IRProtocolInfo spec = {
        "Kaseikyo",
        IREncodingType::PulseDistance,
        37000,
        0.33f,
        3400,
        1750,
        500,
        1300,
        500,
        400,
        500,
        0,
        true,
        0,
        48};
    if (!hideDefaultUI) { displayTextLine("Sending.."); }

    address.replace(" ", "");
    command.replace(" ", "");

    uint32_t addressValue = strtoul(address.c_str(), nullptr, 16);
    uint32_t commandValue = strtoul(command.c_str(), nullptr, 16);

    uint32_t newAddress = swap32(addressValue);
    uint16_t newCommand = static_cast<uint16_t>(swap32(commandValue));

    uint8_t id = (newAddress >> 24) & 0xFF;
    uint16_t vendor_id = (newAddress >> 8) & 0xFFFF;
    uint8_t genre1 = (newAddress >> 4) & 0x0F;
    uint8_t genre2 = newAddress & 0x0F;

    uint16_t data = newCommand & 0x3FF;

    byte bytes[6];
    bytes[0] = vendor_id & 0xFF;
    bytes[1] = (vendor_id >> 8) & 0xFF;

    uint8_t vendor_parity = bytes[0] ^ bytes[1];
    vendor_parity = (vendor_parity & 0xF) ^ (vendor_parity >> 4);

    bytes[2] = (genre1 << 4) | (vendor_parity & 0x0F);
    bytes[3] = ((data & 0x0F) << 4) | genre2;
    bytes[4] = ((id & 0x03) << 6) | ((data >> 4) & 0x3F);

    bytes[5] = bytes[2] ^ bytes[3] ^ bytes[4];

    uint64_t lsb_data = 0;
    for (int i = 0; i < 6; i++) { lsb_data |= (uint64_t)bytes[i] << (8 * i); }

    // LSB First --> MSB First
    uint64_t msb_data = reverseBits(lsb_data, 48);

    std::vector<uint16_t> timings = buildPulseDistanceTimings(msb_data, spec.nbits, spec);
    rmtSendDurations(timings, spec.carrier_hz, spec.duty_cycle);

    if (bruceConfigPins.irTxRepeats > 0) {
        for (uint8_t i = 1; i <= bruceConfigPins.irTxRepeats; i++) {
            rmtSendDurations(timings, spec.carrier_hz, spec.duty_cycle);
        }
    }

    Serial.println(
        "Sent Kaseikyo Command" + (bruceConfigPins.irTxRepeats > 0
                                       ? " (1 initial + " + String(bruceConfigPins.irTxRepeats) + " repeats)"
                                       : "")
    );
    digitalWrite(bruceConfigPins.irTx, LED_OFF);
}

bool sendDecodedCommand(String protocol, String value, uint8_t bits, bool hideDefaultUI) {
    (void)protocol;
    (void)value;
    (void)bits;
    if (!hideDefaultUI) { displayTextLine("Protocol not supported"); }
    delay(500);
    return false;
}

void sendRawCommand(uint16_t frequency, String rawData, bool hideDefaultUI) {
#ifdef USE_BOOST /// ENABLE 5V OUTPUT
    PPM.enableOTG();
#endif

    if (!hideDefaultUI) { displayTextLine("Sending.."); }

    uint16_t dataBufferSize = 1;
    for (int i = 0; i < rawData.length(); i++) {
        if (rawData[i] == ' ') dataBufferSize += 1;
    }
    uint16_t *dataBuffer = (uint16_t *)malloc((dataBufferSize) * sizeof(uint16_t));

    uint16_t count = 0;
    // Parse raw data string
    while (rawData.length() > 0 && count < dataBufferSize) {
        int delimiterIndex = rawData.indexOf(' ');
        if (delimiterIndex == -1) { delimiterIndex = rawData.length(); }
        String dataChunk = rawData.substring(0, delimiterIndex);
        rawData.remove(0, delimiterIndex + 1);
        dataBuffer[count++] = (dataChunk.toInt());
    }

    Serial.println("Parsing raw data complete.");
    // Serial.println(count);
    // Serial.println(dataBuffer[count-1]);
    // Serial.println(dataBuffer[0]);

    std::vector<uint16_t> timings;
    timings.reserve(count);
    for (uint16_t i = 0; i < count; i++) timings.push_back(dataBuffer[i]);
    if (timings.size() % 2 != 0) timings.push_back(1);
    rmtSendDurations(timings, frequency, 0.33f);

    if (bruceConfigPins.irTxRepeats > 0) {
        for (uint8_t i = 1; i <= bruceConfigPins.irTxRepeats; i++) {
            rmtSendDurations(timings, frequency, 0.33f);
        }
    }

    free(dataBuffer);

    Serial.println(
        "Sent Raw Command" + (bruceConfigPins.irTxRepeats > 0
                                  ? " (1 initial + " + String(bruceConfigPins.irTxRepeats) + " repeats)"
                                  : "")
    );
    digitalWrite(bruceConfigPins.irTx, LED_OFF);
}

bool chooseCmdIrFile(FS *fs, String filepath) {
    checkIrTxPin();
    resetCodesArray();
    int total_codes = 0;
    File databaseFile;

    returnToMenu = true;

    databaseFile = fs->open(filepath, FILE_READ);
    drawMainBorder();

    if (!databaseFile) {
        Serial.println("Failed to open IR file.");
        return false;
    }
    Serial.println("Opened IR file.");

    setup_ir_pin(bruceConfigPins.irTx, OUTPUT);

    // Mode to choose and send command by command (limitted to 100 commands)
    String line;
    String txt;
    codes.push_back(new IRCode());

    while (databaseFile.available() && total_codes < 100) {
        line = databaseFile.readStringUntil('\n');
        txt = line.substring(line.indexOf(":") + 1);
        txt.trim();

        if (line.startsWith("name:")) {
            if (codes[total_codes]->name != "") {
                total_codes++;
                codes.push_back(new IRCode());
            }
            // save signal name
            codes[total_codes]->name = txt;
            codes[total_codes]->filepath = txt + " " + filepath.substring(1 + filepath.lastIndexOf("/"));
        }

        if (line.startsWith("type:")) codes[total_codes]->type = txt;
        if (line.startsWith("protocol:")) codes[total_codes]->protocol = txt;
        if (line.startsWith("address:")) codes[total_codes]->address = txt;
        if (line.startsWith("frequency:")) codes[total_codes]->frequency = txt.toInt();
        if (line.startsWith("bits:")) codes[total_codes]->bits = txt.toInt();
        if (line.startsWith("command:")) codes[total_codes]->command = txt;
        if (line.startsWith("data:") || line.startsWith("value:") || line.startsWith("state:")) {
            codes[total_codes]->data = txt;
        }

        if (line.startsWith("#") && total_codes < codes.size() && codes[total_codes]->name != "") {
            total_codes++;
            codes.push_back(new IRCode());
        }
    }

    options = {};
    for (auto code : codes) {
        if (code->name != "") {
            options.push_back({code->name.c_str(), [code]() {
                               sendIRCommand(code);
                               addToRecentCodes(code);
                           }});
        }
    }

    bool exit = false;
    options.push_back({"Main Menu", [&]() { exit = true; }});
    databaseFile.close();

#ifdef USE_BOOST /// DISABLE 5V OUTPUT
    PPM.disableOTG();
#endif

    digitalWrite(bruceConfigPins.irTx, LED_OFF);
    int idx = 0;
    while (1) {
        idx = loopOptions(options, idx);
        if (check(EscPress) || exit) break;
    }
    options.clear();
    resetCodesArray();
    return true;
}
