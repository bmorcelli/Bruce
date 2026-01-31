/**
 * @file ir_read.cpp
 * @author @im.nix (https://github.com/Niximkk)
 * @author Rennan Cockles (https://github.com/rennancockles)
 * @brief Read IR signals
 * @version 0.2
 * @date 2024-08-03
 */

#include "ir_read.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/sd_functions.h"
#include "core/settings.h"
#include "ir_utils.h"
#include <globals.h>
#include <driver/rmt_rx.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <assert.h>
#include <vector>

/* Dont touch this */
// #define MAX_RAWBUF_SIZE 300
#define IR_FREQUENCY 38000
#define DUTY_CYCLE 0.330000

static String build_raw_from_rx(const rmt_rx_done_event_data_t &rx_data) {
    std::vector<uint16_t> durations;
    durations.reserve(rx_data.num_symbols * 2);

    auto append_duration = [&](bool level, uint16_t duration) {
        if (duration == 0) return;
        if (durations.empty() && !level) return; // skip leading spaces

        bool last_is_mark = (durations.size() % 2 == 1);
        if (!durations.empty() && level == last_is_mark) {
            uint32_t merged = durations.back() + duration;
            if (merged > 0xFFFF) merged = 0xFFFF;
            durations.back() = static_cast<uint16_t>(merged);
        } else {
            durations.push_back(duration);
        }
    };

    for (size_t i = 0; i < rx_data.num_symbols; i++) {
        const rmt_symbol_word_t &sym = rx_data.received_symbols[i];
        append_duration(sym.level0, sym.duration0);
        append_duration(sym.level1, sym.duration1);
    }

    if (durations.size() % 2 != 0) durations.push_back(1);

    String signal_code = "";
    for (size_t i = 0; i < durations.size(); i++) {
        signal_code += String(durations[i]);
        if (i + 1 < durations.size()) signal_code += " ";
    }
    signal_code.trim();
    return signal_code;
}

static bool ir_rmt_rx_done_callback(
    rmt_channel_t *channel, const rmt_rx_done_event_data_t *edata, void *user_data
) {
    BaseType_t high_task_wakeup = pdFALSE;
    QueueHandle_t receive_queue = (QueueHandle_t)user_data;
    xQueueSendFromISR(receive_queue, edata, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

String uint32ToString(uint32_t value) {
    char buffer[12] = {0}; // 8 hex digits + 3 spaces + 1 null terminator
    snprintf(
        buffer,
        sizeof(buffer),
        "%02lX %02lX %02lX %02lX",
        value & 0xFF,
        (value >> 8) & 0xFF,
        (value >> 16) & 0xFF,
        (value >> 24) & 0xFF
    );
    return String(buffer);
}

String uint32ToStringInverted(uint32_t value) {
    char buffer[12] = {0}; // 8 hex digits + 3 spaces + 1 null terminator
    snprintf(
        buffer,
        sizeof(buffer),
        "%02lX %02lX %02lX %02lX",
        (value >> 24) & 0xFF,
        (value >> 16) & 0xFF,
        (value >> 8) & 0xFF,
        value & 0xFF
    );
    return String(buffer);
}

IrRead::IrRead(bool headless_mode, bool raw_mode) {
    headless = headless_mode;
    raw = raw_mode;
    setup();
}
bool quickloop = false;

void IrRead::setup() {
#ifdef USE_BOOST /// ENABLE 5V OUTPUT
    PPM.enableOTG();
#endif
    // Checks if irRx pin is properly set
    const std::vector<std::pair<String, int>> pins = IR_RX_PINS;
    int count = 0;
    for (auto pin : pins) {
        if (pin.second == bruceConfigPins.irRx) count++;
    }
    if (count == 0) gsetIrRxPin(true); // Open dialog to choose irRx pin

    setup_ir_pin(bruceConfigPins.irRx, INPUT_PULLUP);
    init_rx();
    if (headless) return;
    // else
    returnToMenu = true; // make sure menu is redrawn when quitting in any point
    std::vector<Option> quickRemoteOptions = {
        {"TV",
         [&]() {
             quickButtons = quickButtonsTV;
             begin();
             return loop();
         }                     },
        {"AC",
         [&]() {
             quickButtons = quickButtonsAC;
             begin();
             return loop();
         }                     },
        {"SOUND",
         [&]() {
             quickButtons = quickButtonsSOUND;
             begin();
             return loop();
         }                     },
        {"LED STRIP", [&]() {
             quickButtons = quickButtonsLED;
             begin();
             return loop();
         }},
    };
    options = {
        {"Custom Read",
         [&]() {
             begin();
             return loop();
         }                            },
        {"Quick Remote Setup  ",
         [&]() {
             quickloop = true;
             loopOptions(quickRemoteOptions);
         }                            },
        {"Menu",                 yield},
    };
    loopOptions(options);
}

void IrRead::loop() {
    while (1) {
        if (check(EscPress)) {
            returnToMenu = true;
            button_pos = 0;
            quickloop = false;
#ifdef USE_BOOST /// DISABLE 5V OUTPUT
            PPM.disableOTG();
#endif
            deinit_rx();
            break;
        }
        if (check(NextPress)) save_signal();
        if (quickloop && button_pos == quickButtons.size()) save_device();
        if (check(SelPress)) save_device();
        if (check(PrevPress)) discard_signal();

        read_signal();
    }
}

void IrRead::begin() {
    _read_signal = false;

    display_banner();
    if (quickloop) {
        padprintln("Waiting for signal of button: " + String(quickButtons[button_pos]));
    } else {
        padprintln("Waiting for signal...");
    }

    tft.println("");
    display_btn_options();

    delay(300);
}

void IrRead::cls() {
    drawMainBorder();
    tft.setCursor(10, 28);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
}

void IrRead::display_banner() {
    cls();
    tft.setTextSize(FM);
    padprintln("IR Read");

    tft.setTextSize(FP);
    padprintln("--------------");
    padprintln("Signals captured: " + String(signals_read));
    tft.println("");
}

void IrRead::display_btn_options() {
    tft.println("");
    tft.println("");
    if (_read_signal) {
        padprintln("Press [PREV] to discard signal");
        padprintln("Press [NEXT] to save signal");
    }
    if (signals_read > 0) { padprintln("Press [OK]   to save device"); }
    padprintln("Press [ESC]  to exit");
}

void IrRead::read_signal() {
    if (_read_signal || rx_queue == NULL) return;

    rmt_rx_done_event_data_t rx_data;
    if (xQueueReceive(rx_queue, &rx_data, 0) != pdPASS) return;

    last_raw_signal = build_raw_from_rx(rx_data);
    _read_signal = true;

    // Always switches to RAW data, regardless of the decoding result
    raw = true;

    display_banner();

    // Dump of signal details
    padprint("RAW Data Captured:");
    String raw_signal = parse_raw_signal();
    tft.println(
        raw_signal.substring(0, 45) + (raw_signal.length() > 45 ? "..." : "")
    ); // Shows the RAW signal on the display

    display_btn_options();
    delay(500);
}

void IrRead::discard_signal() {
    if (!_read_signal) return;
    restart_rx();
    begin();
}

void IrRead::save_signal() {
    if (!_read_signal) return;
    if (!quickloop) {
        String btn_name = keyboard("Btn" + String(signals_read), 30, "Btn name:");
        append_to_file_str(btn_name);
    } else {
        append_to_file_str(quickButtons[button_pos]);
    }
    signals_read++;
    if (quickloop) button_pos++;
    discard_signal();
    delay(100);
}

String IrRead::parse_raw_signal() {
    return last_raw_signal;
}

void IrRead::init_rx() {
    if (rx_channel != NULL) return;

    rmt_rx_channel_config_t rx_channel_cfg = {};
    rx_channel_cfg.gpio_num = gpio_num_t(bruceConfigPins.irRx);
    rx_channel_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
    rx_channel_cfg.resolution_hz = 1000000; // 1 tick = 1us
    rx_channel_cfg.mem_block_symbols = 64;
    rx_channel_cfg.intr_priority = 0;
    rx_channel_cfg.flags.invert_in = true;  // IR demodulators are typically active-low
    rx_channel_cfg.flags.with_dma = false;
    rx_channel_cfg.flags.allow_pd = false;
    rx_channel_cfg.flags.io_loop_back = false;

    ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_channel_cfg, &rx_channel));

    rx_queue = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));
    assert(rx_queue);

    rmt_rx_event_callbacks_t cbs = {
        .on_recv_done = ir_rmt_rx_done_callback,
    };
    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(rx_channel, &cbs, rx_queue));
    ESP_ERROR_CHECK(rmt_enable(rx_channel));

    rx_config.signal_range_min_ns = 1000;      // 1us
    rx_config.signal_range_max_ns = 30000000;  // 30ms
    restart_rx();
}

void IrRead::restart_rx() {
    if (rx_channel == NULL) return;
    ESP_ERROR_CHECK(rmt_receive(rx_channel, rx_buffer, sizeof(rx_buffer), &rx_config));
    _read_signal = false;
}

void IrRead::deinit_rx() {
    if (rx_channel == NULL) return;
    rmt_disable(rx_channel);
    rmt_del_channel(rx_channel);
    rx_channel = NULL;
    if (rx_queue != NULL) {
        vQueueDelete(rx_queue);
        rx_queue = NULL;
    }
}

void IrRead::append_to_file_str(String btn_name) {
    strDeviceContent += "name: " + btn_name + "\n";
    strDeviceContent += "type: raw\n";
    strDeviceContent += "frequency: " + String(IR_FREQUENCY) + "\n";
    strDeviceContent += "duty_cycle: " + String(DUTY_CYCLE) + "\n";
    strDeviceContent += "data: " + parse_raw_signal() + "\n";
    strDeviceContent += "#\n";
}

void IrRead::save_device() {
    if (signals_read == 0) return;

    String filename = keyboard("MyDevice", 30, "File name:");

    display_banner();

    FS *fs = nullptr;

    bool sdCardAvailable = setupSdCard();
    bool littleFsAvailable = checkLittleFsSize();

    if (sdCardAvailable && littleFsAvailable) {
        // ask to choose one
        options = {
            {"SD Card",  [&]() { fs = &SD; }      },
            {"LittleFS", [&]() { fs = &LittleFS; }},
        };

        loopOptions(options);
    } else if (sdCardAvailable) {
        fs = &SD;
    } else if (littleFsAvailable) {
        fs = &LittleFS;
    };

    if (fs && write_file(filename, fs)) {
        displaySuccess("File saved to " + String((fs == &SD) ? "SD Card" : "LittleFS") + ".", true);
        signals_read = 0;
        strDeviceContent = "";
    } else displayError(fs ? "Error writing file." : "No storage available.", true);

    delay(1000);

    restart_rx();
    begin();
}

String IrRead::loop_headless(int max_loops) {
    if (rx_queue == NULL) init_rx();
    rmt_rx_done_event_data_t rx_data;
    while (xQueueReceive(rx_queue, &rx_data, pdMS_TO_TICKS(1000)) != pdPASS) {
        max_loops -= 1;
        if (max_loops <= 0) {
            Serial.println("timeout");
            return "";
        }
    }

    raw = true;
    last_raw_signal = build_raw_from_rx(rx_data);

    if (rx_data.num_symbols >= (sizeof(rx_buffer) / sizeof(rx_buffer[0])) - 1) {
        displayWarning("buffer overflow, data may be truncated", true);
    }

    String r = "Filetype: IR signals file\n";
    r += "Version: 1\n";
    r += "#\n";
    r += "#\n";

    strDeviceContent = "";
    append_to_file_str("Unknown"); // writes on strDeviceContent
    r += strDeviceContent;

    restart_rx();
    return r;
}

bool IrRead::write_file(String filename, FS *fs) {
    if (fs == nullptr) return false;

    if (!(*fs).exists("/BruceIR")) (*fs).mkdir("/BruceIR");

    while ((*fs).exists("/BruceIR/" + filename + ".ir")) {
        int ch = 1;
        int i = 1;

        displayWarning("File \"" + String(filename) + "\" already exists", true);
        display_banner();

        // ask to choose one
        options = {
            {"Append number", [&]() { ch = 1; }},
            {"Overwrite ",    [&]() { ch = 2; }},
            {"Change name",   [&]() { ch = 3; }},
        };

        loopOptions(options);

        switch (ch) {
            case 1:
                filename += "_";
                while ((*fs).exists("/BruceIR/" + filename + String(i) + ".ir")) i++;
                filename += String(i);
                break;
            case 2: (*fs).remove("/BruceIR/" + filename + ".ir"); break;
            case 3:
                filename = keyboard(filename, 30, "File name:");
                display_banner();
                break;
        }
    }

    /*
    /Old "Add num index" solution

    if ((*fs).exists("/BruceIR/" + filename + ".ir")) {
        int i = 1;
        filename += "_";
        while((*fs).exists("/BruceIR/" + filename + String(i) + ".ir")) i++;
        filename += String(i);
    }
    */

    File file = (*fs).open("/BruceIR/" + filename + ".ir", FILE_WRITE);

    if (!file) { return false; }

    file.println("Filetype: Bruce IR File");
    file.println("Version: 1");
    file.println("#");
    file.println("# " + filename);
    file.print(strDeviceContent);

    file.close();
    delay(100);
    return true;
}
