#include "ST25R3916.h"
#if !defined(LITE_VERSION)

// ST25R3916 via RFAL fork (lewisxhe/ST25R3916-fork + NFC-RFAL-fork).
// Supported: SPI mode on boards with ST25R wiring such as lilygo-t-lora-pager and reaper.
// I2C mode uses the fork constructor RfalRfST25R3916Class(&Wire, irqPin).
// Card emulation/listen mode is not wired in this driver yet.
// MIFARE Classic and ISO15693 writes are not implemented.

#include "core/sd_functions.h"
#include "modules/rfid/apdu.h"
#include <globals.h>

#define ST25R_DEBUG 1
#if ST25R_DEBUG
#define ST25R_LOG(fmt, ...) Serial.printf("[ST25R] " fmt "\n", ##__VA_ARGS__)
#else
#define ST25R_LOG(fmt, ...)                                                                                  \
    do {                                                                                                     \
    } while (0)
#endif

static const char *_stateStr(rfalNfcState st) {
    switch (st) {
        case RFAL_NFC_STATE_NOTINIT: return "NOTINIT";
        case RFAL_NFC_STATE_IDLE: return "IDLE";
        case RFAL_NFC_STATE_START_DISCOVERY: return "START_DISCOVERY";
        case RFAL_NFC_STATE_WAKEUP_MODE: return "WAKEUP_MODE";
        case RFAL_NFC_STATE_POLL_TECHDETECT: return "POLL_TECHDETECT";
        case RFAL_NFC_STATE_POLL_COLAVOIDANCE: return "POLL_COLAVOIDANCE";
        case RFAL_NFC_STATE_POLL_SELECT: return "POLL_SELECT";
        case RFAL_NFC_STATE_POLL_ACTIVATION: return "POLL_ACTIVATION";
        case RFAL_NFC_STATE_LISTEN_TECHDETECT: return "LISTEN_TECHDETECT";
        case RFAL_NFC_STATE_ACTIVATED: return "ACTIVATED";
        case RFAL_NFC_STATE_DATAEXCHANGE: return "DATAEXCHANGE";
        case RFAL_NFC_STATE_DEACTIVATION: return "DEACTIVATION";
        default: return "UNKNOWN";
    }
}

static void _deselectSharedSpiDevices() {
    /*
    #if defined(TFT_CS) && TFT_CS >= 0
        digitalWrite(TFT_CS, HIGH);
    #endif
    #if defined(SDCARD_CS) && SDCARD_CS >= 0
        digitalWrite(SDCARD_CS, HIGH);
    #endif
    #if defined(LORA_CS) && LORA_CS >= 0
        digitalWrite(LORA_CS, HIGH);
    #endif
    #if defined(NFC_CS) && NFC_CS >= 0
        digitalWrite(NFC_CS, HIGH);
    #endif
    */
}

static void _setNfcPower(bool enabled) {
#if defined(IO_EXP_NFC) && IO_EXP_NFC >= 0
    ioExpander.setPinDirection(IO_EXP_NFC, OUTPUT);
    ioExpander.turnPinOnOff(IO_EXP_NFC, enabled ? HIGH : LOW);
#endif
}

namespace {
int st25HexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool st25ParseHexBytesAfterColon(const String &line, std::vector<uint8_t> &bytes) {
    bytes.clear();
    int colon = line.indexOf(':');
    if (colon < 0) return false;

    int hi = -1;
    for (int i = colon + 1; i < line.length(); i++) {
        int v = st25HexNibble(line.charAt(i));
        if (v < 0) continue;
        if (hi < 0) {
            hi = v;
        } else {
            bytes.push_back(static_cast<uint8_t>((hi << 4) | v));
            hi = -1;
        }
    }
    return !bytes.empty() && hi < 0;
}

bool st25ExtractNdefMessageFromPageDump(const String &dump, std::vector<uint8_t> &ndefOut) {
    ndefOut.clear();
    if (dump.length() == 0) return false;

    std::vector<uint8_t> userData;
    std::vector<uint8_t> lineBytes;
    int pos = 0;

    while (pos < dump.length()) {
        int nl = dump.indexOf('\n', pos);
        if (nl < 0) nl = dump.length();

        String line = dump.substring(pos, nl);
        line.trim();
        pos = nl + 1;

        if (!line.startsWith("Page ")) continue;

        int colon = line.indexOf(':');
        if (colon < 0) continue;

        int page = line.substring(5, colon).toInt();
        if (page < 4) continue;

        if (!st25ParseHexBytesAfterColon(line, lineBytes)) continue;
        if (lineBytes.size() < 4) continue;
        userData.insert(userData.end(), lineBytes.begin(), lineBytes.begin() + 4);
    }

    size_t i = 0;
    while (i < userData.size()) {
        uint8_t tlv = userData[i++];

        if (tlv == 0x00) continue;
        if (tlv == 0xFE) break;
        if (i >= userData.size()) return false;

        uint32_t len = userData[i++];
        if (len == 0xFF) {
            if (i + 1 >= userData.size()) return false;
            len = (static_cast<uint32_t>(userData[i]) << 8) | userData[i + 1];
            i += 2;
        }

        if (i + len > userData.size()) return false;

        if (tlv == 0x03) {
            ndefOut.assign(userData.begin() + i, userData.begin() + i + len);
            return !ndefOut.empty();
        }

        i += len;
    }

    return false;
}

bool st25BuildNdefMessageFromStruct(const RFIDInterface::NdefMessage &src, std::vector<uint8_t> &ndefOut) {
    ndefOut.clear();
    if (src.messageSize == 0 || src.payloadSize == 0) return false;

    ndefOut.reserve(4 + src.payloadSize);
    ndefOut.push_back(src.header);
    ndefOut.push_back(src.tnf);
    ndefOut.push_back(src.payloadSize);
    ndefOut.push_back(src.payloadType);
    ndefOut.insert(ndefOut.end(), src.payload, src.payload + src.payloadSize);

    return ndefOut.size() == src.messageSize;
}
} // namespace

void ST25R3916::_logOpControl(const char *where) {
    if (!_hw) return;

    uint8_t op = 0;
    uint8_t id = 0;
    uint8_t io1 = 0;
    uint8_t io2 = 0;
    _hw->st25r3916ReadRegister(ST25R3916_REG_OP_CONTROL, &op);
    _hw->st25r3916ReadRegister(ST25R3916_REG_IC_IDENTITY, &id);
    _hw->st25r3916ReadRegister(ST25R3916_REG_IO_CONF1, &io1);
    _hw->st25r3916ReadRegister(ST25R3916_REG_IO_CONF2, &io2);
    ST25R_LOG(
        "%s OP_CONTROL=0x%02X en=%d rx=%d tx=%d IC_ID=0x%02X IO1=0x%02X IO2=0x%02X",
        where,
        op,
        (op & ST25R3916_REG_OP_CONTROL_en) ? 1 : 0,
        (op & ST25R3916_REG_OP_CONTROL_rx_en) ? 1 : 0,
        (op & ST25R3916_REG_OP_CONTROL_tx_en) ? 1 : 0,
        id,
        io1,
        io2
    );
}

void ST25R3916::_probeField(const char *where) {
    if (!_hw) return;

    auto err = _hw->rfalFieldOnAndStartGT();
    ST25R_LOG("%s rfalFieldOnAndStartGT -> %d", where, err);
    _logOpControl("after manual field on");
    _hw->rfalFieldOff();
    _logOpControl("after manual field off");
}

ST25R3916::ST25R3916(CONNECTION_TYPE connection_type) : _connection_type(connection_type) {}

ST25R3916::~ST25R3916() {
    stopDiscovery();
    _setNfcPower(false);
    delete _nfc;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
    // RFAL class is deleted through its concrete type, but the fork lacks a virtual destructor.
    delete _hw;
#pragma GCC diagnostic pop
}

bool ST25R3916::_initSPI() {
    _deselectSharedSpiDevices();

    int cs = (int)bruceConfigPins.ST25R_bus.cs;
    int irq = (int)bruceConfigPins.ST25R_bus.io0;
    ST25R_LOG(
        "_initSPI: CS=%d IRQ=%d MOSI=%d MISO=%d SCK=%d",
        cs,
        irq,
        (int)bruceConfigPins.ST25R_bus.mosi,
        (int)bruceConfigPins.ST25R_bus.miso,
        (int)bruceConfigPins.ST25R_bus.sck
    );

    if (bruceConfigPins.ST25R_bus.mosi == (gpio_num_t)TFT_MOSI &&
        bruceConfigPins.ST25R_bus.mosi != GPIO_NUM_NC) {
        ST25R_LOG("_initSPI: sharing TFT SPI");
        _spi = &tft.getSPIinstance();
    } else if (bruceConfigPins.ST25R_bus.mosi == bruceConfigPins.SDCARD_bus.mosi) {
        ST25R_LOG("_initSPI: sharing SDCARD SPI");
        _spi = &sdcardSPI;
    } else {
        ST25R_LOG("_initSPI: creating dedicated HSPI");
        static SPIClass st25rSPI(HSPI);
        static bool spiStarted = false;
        if (!spiStarted) {
            st25rSPI.begin(
                (int)bruceConfigPins.ST25R_bus.sck,
                (int)bruceConfigPins.ST25R_bus.miso,
                (int)bruceConfigPins.ST25R_bus.mosi
            );
            spiStarted = true;
        }
        _spi = &st25rSPI;
    }

    _hw = new RfalRfST25R3916Class(_spi, cs, irq);
    ST25R_LOG("_initSPI: _hw=%s", _hw ? "ok" : "NULL");
    return _hw != nullptr;
}

bool ST25R3916::_initI2C() {
    int irq = (int)bruceConfigPins.ST25R_bus.io0;
    Wire.begin(bruceConfigPins.i2c_bus.sda, bruceConfigPins.i2c_bus.scl);
    _hw = new RfalRfST25R3916Class(&Wire, irq);
    return _hw != nullptr;
}

bool ST25R3916::begin() {
    _setNfcPower(true);
    delay(5);
    _deselectSharedSpiDevices();
    ST25R_LOG("begin: mode=%s", _connection_type == SPI_MODE ? "SPI" : "I2C");
    bool ok = (_connection_type == SPI_MODE) ? _initSPI() : _initI2C();
    if (!ok) {
        ST25R_LOG("begin: hw init FAILED");
        return false;
    }

    _nfc = new RfalNfcClass(_hw);
    uint16_t err = _nfc->rfalNfcInitialize();
    ST25R_LOG("begin: rfalNfcInitialize -> %d (%s)", err, err == ST_ERR_NONE ? "OK" : "FAIL");
    _logOpControl("after init");
    _hw->st25r3916OscOn();
    _logOpControl("after forced osc on");
    return (err == ST_ERR_NONE);
}

static void _notifyCb(rfalNfcState st) { ST25R_LOG("notifyCb: %s(%d)", _stateStr(st), (int)st); }

bool ST25R3916::_startDiscovery() {
    _deselectSharedSpiDevices();

    rfalNfcDiscoverParam params;
    memset(&params, 0, sizeof(params));
    params.compMode = RFAL_COMPLIANCE_MODE_NFC;
    params.devLimit = 1;
    params.nfcfBR = RFAL_BR_212;
    params.ap2pBR = RFAL_BR_424;
    params.techs2Find = RFAL_NFC_POLL_TECH_A | RFAL_NFC_POLL_TECH_B | RFAL_NFC_POLL_TECH_V;
    params.GBLen = RFAL_NFCDEP_GB_MAX_LEN;
    params.notifyCb = _notifyCb;
    params.totalDuration = 1000U;
    params.wakeupEnabled = false;
    params.wakeupConfigDefault = true;

    uint16_t err = _nfc->rfalNfcDiscover(&params);
    ST25R_LOG("_startDiscovery: rfalNfcDiscover -> %d (%s)", err, err == ST_ERR_NONE ? "OK" : "FAIL");
    _hw->st25r3916OscOn();
    _logOpControl("after discover");
    return (err == ST_ERR_NONE);
}

void ST25R3916::stopDiscovery() {
    if (_discoveryStarted && _nfc) {
        _nfc->rfalNfcDeactivate(false);
        _discoveryStarted = false;
    }
}

String ST25R3916::_getNfcaTypeName(uint8_t sak) {
    if (sak == 0x08 || sak == 0x88) return "MIFARE Classic 1K";
    if (sak == 0x18) return "MIFARE Classic 4K";
    if (sak == 0x09) return "MIFARE Mini";
    if (sak == 0x00) return "MIFARE Ultralight";
    if (sak == 0x20) return "ISO 14443-4";
    return "ISO 14443A";
}

void ST25R3916::_parseDevice(rfalNfcDevice *dev) {
    uid.size = dev->nfcidLen;
    memcpy(uid.uidByte, dev->nfcid, uid.size);

    printableUID.uid = "";
    for (int i = 0; i < uid.size; i++) {
        char buf[3];
        sprintf(buf, "%02X", uid.uidByte[i]);
        printableUID.uid += buf;
        if (i < uid.size - 1) printableUID.uid += " ";
    }

    uint8_t bcc = 0;
    for (int i = 0; i < uid.size; i++) bcc ^= uid.uidByte[i];
    char bccBuf[3];
    sprintf(bccBuf, "%02X", bcc);
    printableUID.bcc = String(bccBuf);

    switch (dev->type) {
        case RFAL_NFC_LISTEN_TYPE_NFCA: {
            rfalNfcaListenDevice *nfca = &dev->dev.nfca;
            uid.sak = nfca->selRes.sak;
            char sakBuf[3];
            sprintf(sakBuf, "%02X", uid.sak);
            printableUID.sak = String(sakBuf);

            uid.atqaByte[0] = nfca->sensRes.anticollisionInfo;
            uid.atqaByte[1] = nfca->sensRes.platformInfo;
            char atqaBuf[6];
            sprintf(atqaBuf, "%02X %02X", uid.atqaByte[1], uid.atqaByte[0]);
            printableUID.atqa = String(atqaBuf);

            printableUID.picc_type = _getNfcaTypeName(uid.sak);
            break;
        }
        case RFAL_NFC_LISTEN_TYPE_NFCB:
            printableUID.picc_type = "ISO14443B";
            printableUID.sak = "--";
            printableUID.atqa = "--";
            break;
        case RFAL_NFC_LISTEN_TYPE_NFCV:
            printableUID.picc_type = "ISO15693";
            printableUID.sak = "--";
            printableUID.atqa = "--";
            break;
        default:
            printableUID.picc_type = "Unknown";
            printableUID.sak = "--";
            printableUID.atqa = "--";
            break;
    }
}

int ST25R3916::_readDataBlocks(rfalNfcDevice *dev) {
    strAllPages = "";
    dataPages = 0;
    totalPages = 0;
    pageReadSuccess = false;

    if (dev->type != RFAL_NFC_LISTEN_TYPE_NFCA || uid.sak != 0x00) {
        pageReadStatus = FAILURE;
        return FAILURE;
    }

    // Detect NTAG size from CC page (page 3)
    uint8_t ccBuf[16] = {0};
    uint16_t rcvLen = 0;
    auto ccErr = _nfc->rfalT2TPollerRead(3, ccBuf, sizeof(ccBuf), &rcvLen);
    if (ccErr != ST_ERR_NONE || rcvLen < 4) {
        pageReadStatus = FAILURE;
        return FAILURE;
    }
    switch (ccBuf[2]) {
        case 0x12: totalPages = 45; break;  // NTAG213
        case 0x3E: totalPages = 135; break; // NTAG215
        case 0x6D: totalPages = 231; break; // NTAG216
        default: totalPages = 64; break;    // MF Ultralight
    }

    for (int page = 0; page < totalPages; page++) {
        uint8_t pageData[16] = {0};
        uint16_t len = 0;
        auto err = _nfc->rfalT2TPollerRead(page, pageData, sizeof(pageData), &len);
        if (err != ST_ERR_NONE || len < 4) {
            pageReadStatus = FAILURE;
            return FAILURE;
        }
        char line[32];
        sprintf(
            line, "Page %d: %02X %02X %02X %02X", page, pageData[0], pageData[1], pageData[2], pageData[3]
        );
        strAllPages += String(line) + "\n";
        dataPages++;
    }

    pageReadStatus = SUCCESS;
    pageReadSuccess = true;
    return SUCCESS;
}

void ST25R3916::_parseLoadedData() {
    String strUID = printableUID.uid;
    strUID.trim();
    strUID.replace(" ", "");
    uid.size = strUID.length() / 2;
    if (uid.size > sizeof(uid.uidByte)) uid.size = sizeof(uid.uidByte);
    for (size_t i = 0; i < uid.size; i++) {
        uid.uidByte[i] = strtoul(strUID.substring(i * 2, i * 2 + 2).c_str(), NULL, 16);
    }

    uint8_t bcc = 0;
    for (int i = 0; i < uid.size; i++) bcc ^= uid.uidByte[i];
    char bccBuf[3];
    sprintf(bccBuf, "%02X", bcc);
    printableUID.bcc = String(bccBuf);

    printableUID.sak.trim();
    uid.sak = strtoul(printableUID.sak.c_str(), NULL, 16);

    String strAtqa = printableUID.atqa;
    strAtqa.trim();
    strAtqa.replace(" ", "");
    uid.atqaByte[0] = 0;
    uid.atqaByte[1] = 0;
    if (strAtqa.length() >= 4) {
        uid.atqaByte[1] = strtoul(strAtqa.substring(0, 2).c_str(), NULL, 16);
        uid.atqaByte[0] = strtoul(strAtqa.substring(2, 4).c_str(), NULL, 16);
    }
}

bool ST25R3916::_isUltralightUserPage(int page) const {
    if (page < 4) return false;
    if (totalPages <= 0) return true;
    return page < (totalPages - 5);
}

bool ST25R3916::_buildLoadedNdefMessage(std::vector<uint8_t> &ndefOut) {
    if (st25ExtractNdefMessageFromPageDump(strAllPages, ndefOut)) return true;
    if (st25BuildNdefMessageFromStruct(ndefMessage, ndefOut)) return true;

    std::vector<uint8_t> uriPayload = Ndef::urlNdefAbbrv("https://bruce.computer");
    ndefOut = Ndef::newMessage(uriPayload);
    return !ndefOut.empty();
}

int ST25R3916::read(int cardBaudRate) {
    pageReadStatus = FAILURE;
    pageReadSuccess = false;
    if (!_nfc) return FAILURE;
    _deselectSharedSpiDevices();

    if (!_discoveryStarted) {
        ST25R_LOG("starting discovery");
        if (!_startDiscovery()) {
            ST25R_LOG("_startDiscovery FAILED");
            return FAILURE;
        }
        _discoveryStarted = true;
    }

    _deselectSharedSpiDevices();
    _hw->st25r3916OscOn();
    _nfc->rfalNfcWorker();

    rfalNfcState state = _nfc->rfalNfcGetState();
    ST25R_LOG("-> %s(%d)", _stateStr(state), (int)state);

    if (state != RFAL_NFC_STATE_ACTIVATED) { return TAG_NOT_PRESENT; }

    rfalNfcDevice *dev;
    _nfc->rfalNfcGetActiveDevice(&dev);
    ST25R_LOG("TAG! type=%d uid_len=%d", (int)dev->type, dev->nfcidLen);
    _parseDevice(dev);
    ST25R_LOG(
        "uid=%s type=%s sak=%s atqa=%s",
        printableUID.uid.c_str(),
        printableUID.picc_type.c_str(),
        printableUID.sak.c_str(),
        printableUID.atqa.c_str()
    );
    int blkResult = _readDataBlocks(dev);
    ST25R_LOG("_readDataBlocks -> %d pages=%d", blkResult, dataPages);
    _nfc->rfalNfcDeactivate(false);
    _discoveryStarted = false;
    return SUCCESS;
}

bool ST25R3916::_pollForTag(rfalNfcDevice **dev, uint32_t timeoutMs) {
    if (_discoveryStarted) {
        _nfc->rfalNfcDeactivate(false);
        _discoveryStarted = false;
    }
    _deselectSharedSpiDevices();

    rfalNfcDiscoverParam params;
    memset(&params, 0, sizeof(params));
    params.compMode = RFAL_COMPLIANCE_MODE_NFC;
    params.devLimit = 1;
    params.nfcfBR = RFAL_BR_212;
    params.ap2pBR = RFAL_BR_424;
    params.techs2Find = RFAL_NFC_POLL_TECH_A;
    params.GBLen = RFAL_NFCDEP_GB_MAX_LEN;
    params.notifyCb = _notifyCb;
    params.totalDuration = timeoutMs;
    params.wakeupEnabled = false;
    params.wakeupConfigDefault = true;

    if (_nfc->rfalNfcDiscover(&params) != ST_ERR_NONE) return false;
    _hw->st25r3916OscOn();

    uint32_t deadline = millis() + timeoutMs;
    while (millis() < deadline) {
        _nfc->rfalNfcWorker();
        if (_nfc->rfalNfcGetState() == RFAL_NFC_STATE_ACTIVATED) {
            _nfc->rfalNfcGetActiveDevice(dev);
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    return false;
}

bool ST25R3916::_writeT2TPage(uint8_t page, const uint8_t data[4], bool verify) {
    auto err = _nfc->rfalT2TPollerWrite(page, const_cast<uint8_t *>(data));
    if (err != ST_ERR_NONE) {
        ST25R_LOG("_writeT2TPage: page %u write failed err=%d", page, (int)err);
        return false;
    }

    delay(12);

    if (!verify) return true;

    uint8_t rx[16] = {0};
    uint16_t rxLen = 0;
    err = _nfc->rfalT2TPollerRead(page, rx, sizeof(rx), &rxLen);
    if (err != ST_ERR_NONE || rxLen < 4) {
        ST25R_LOG("_writeT2TPage: page %u verify read failed err=%d len=%u", page, (int)err, rxLen);
        return false;
    }

    if (memcmp(rx, data, 4) != 0) {
        ST25R_LOG(
            "_writeT2TPage: page %u verify mismatch got=%02X %02X %02X %02X expected=%02X %02X %02X %02X",
            page,
            rx[0],
            rx[1],
            rx[2],
            rx[3],
            data[0],
            data[1],
            data[2],
            data[3]
        );
        return false;
    }

    return true;
}

int ST25R3916::_writeUltralight(rfalNfcDevice *dev) {
    String pages = strAllPages;
    int written = 0;
    int pos = 0;

    while (pos < (int)pages.length()) {
        int nl = pages.indexOf('\n', pos);
        if (nl < 0) nl = pages.length();
        String line = pages.substring(pos, nl);
        pos = nl + 1;
        if (line.length() == 0) continue;

        int pageNum = -1;
        unsigned int b0 = 0, b1 = 0, b2 = 0, b3 = 0;
        if (sscanf(line.c_str(), "Page %d: %02X %02X %02X %02X", &pageNum, &b0, &b1, &b2, &b3) == 5) {
            if (!_isUltralightUserPage(pageNum)) continue;
            uint8_t data[4] = {(uint8_t)b0, (uint8_t)b1, (uint8_t)b2, (uint8_t)b3};
            if (!_writeT2TPage((uint8_t)pageNum, data)) {
                _nfc->rfalNfcDeactivate(false);
                return FAILURE;
            }
            written++;
        }
    }

    ST25R_LOG("_writeUltralight: wrote %d pages", written);
    _nfc->rfalNfcDeactivate(false);
    return SUCCESS;
}

int ST25R3916::_eraseUltralight(rfalNfcDevice *dev) {
    uint8_t zeros[4] = {0x00, 0x00, 0x00, 0x00};
    for (int page = 4; page < totalPages; page++) {
        if (!_isUltralightUserPage(page)) continue;
        if (!_writeT2TPage((uint8_t)page, zeros)) {
            _nfc->rfalNfcDeactivate(false);
            return FAILURE;
        }
    }
    ST25R_LOG("_eraseUltralight: erased pages 4..%d", totalPages - 1);
    _nfc->rfalNfcDeactivate(false);
    return SUCCESS;
}

int ST25R3916::_writeNdefBlocks(rfalNfcDevice *dev) {
    uint8_t ndefPayload[180] = {0};
    size_t idx = 0;

    if (ndefMessage.messageSize == 0 || ndefMessage.payloadSize == 0) return FAILURE;
    if ((size_t)ndefMessage.messageSize + 3 > sizeof(ndefPayload)) return FAILURE;

    ndefPayload[idx++] = ndefMessage.begin;
    ndefPayload[idx++] = ndefMessage.messageSize;
    ndefPayload[idx++] = ndefMessage.header;
    ndefPayload[idx++] = ndefMessage.tnf;
    ndefPayload[idx++] = ndefMessage.payloadSize;
    ndefPayload[idx++] = ndefMessage.payloadType;

    for (uint8_t i = 0; i < ndefMessage.payloadSize && idx < sizeof(ndefPayload); i++) {
        ndefPayload[idx++] = ndefMessage.payload[i];
    }

    if (idx >= sizeof(ndefPayload)) return FAILURE;
    ndefPayload[idx++] = ndefMessage.end;

    int maxBytes = (totalPages > 0 ? (totalPages - 5 - 4) : 36) * 4;
    if (maxBytes <= 0 || (int)idx > maxBytes) {
        ST25R_LOG("_writeNdefBlocks: payload too large len=%d max=%d", (int)idx, maxBytes);
        return FAILURE;
    }

    int pagesWritten = 0;
    for (int page = 4; _isUltralightUserPage(page); page++) {
        int base = (page - 4) * 4;
        if (base >= (int)idx) break;

        uint8_t data[4] = {0x00, 0x00, 0x00, 0x00};
        for (int b = 0; b < 4 && base + b < (int)idx; b++) data[b] = ndefPayload[base + b];

        if (!_writeT2TPage((uint8_t)page, data)) {
            return FAILURE;
        }
        pagesWritten++;
    }

    uint8_t zeros[4] = {0x00, 0x00, 0x00, 0x00};
    for (int page = 4 + pagesWritten; _isUltralightUserPage(page); page++) {
        if (!_writeT2TPage((uint8_t)page, zeros)) {
            return FAILURE;
        }
    }

    ST25R_LOG("_writeNdefBlocks: wrote %d bytes", (int)idx);
    return SUCCESS;
}

bool ST25R3916::_writeMagicGen2UID(rfalNfcDevice *dev) {
    if (uid.size == 7) {
        uint8_t bcc0 = 0x88 ^ uid.uidByte[0] ^ uid.uidByte[1] ^ uid.uidByte[2];
        uint8_t bcc1 = uid.uidByte[3] ^ uid.uidByte[4] ^ uid.uidByte[5] ^ uid.uidByte[6];
        uint8_t page0[4] = {uid.uidByte[0], uid.uidByte[1], uid.uidByte[2], bcc0};
        uint8_t page1[4] = {uid.uidByte[3], uid.uidByte[4], uid.uidByte[5], uid.uidByte[6]};
        uint8_t page2[4] = {bcc1, 0x00, 0x00, 0x00};
        if (_writeT2TPage(0, page0) && _writeT2TPage(1, page1) && _writeT2TPage(2, page2)) {
            ST25R_LOG("_writeMagicGen2UID: 7-byte UID written to pages 0-2");
            return true;
        }
    } else if (uid.size == 4) {
        uint8_t bcc = uid.uidByte[0] ^ uid.uidByte[1] ^ uid.uidByte[2] ^ uid.uidByte[3];
        uint8_t page0[4] = {uid.uidByte[0], uid.uidByte[1], uid.uidByte[2], uid.uidByte[3]};
        uint8_t page1[4] = {bcc, uid.sak, uid.atqaByte[1], uid.atqaByte[0]};
        if (_writeT2TPage(0, page0) && _writeT2TPage(1, page1)) {
            ST25R_LOG("_writeMagicGen2UID: 4-byte UID written to pages 0-1");
            return true;
        }
    }
    return false;
}

bool ST25R3916::_writeMagicGen1UID(rfalNfcDevice *dev) {
    // Magic Gen1 backdoor: send 0x40 (7 bits, no CRC, no parity), then 0x43
    uint8_t cmd40 = 0x40;
    uint8_t rxBuf[4] = {0};
    uint16_t actLen = 0;

    rfalTransceiveContext ctx;
    ctx.txBuf = &cmd40;
    ctx.txBufLen = 7;
    ctx.rxBuf = rxBuf;
    ctx.rxBufLen = 32;
    ctx.rxRcvdLen = &actLen;
    ctx.flags = (uint32_t)RFAL_TXRX_FLAGS_CRC_TX_MANUAL | (uint32_t)RFAL_TXRX_FLAGS_PAR_TX_NONE |
                (uint32_t)RFAL_TXRX_FLAGS_CRC_RX_KEEP | (uint32_t)RFAL_TXRX_FLAGS_PAR_RX_KEEP;
    ctx.fwt = rfalConv64fcTo1fc(5000);

    if (_hw->rfalStartTransceive(&ctx) != ST_ERR_NONE) return false;
    uint32_t t0 = millis();
    while (millis() - t0 < 50) {
        _hw->rfalWorker();
        if (_hw->rfalGetTransceiveState() == RFAL_TXRX_STATE_IDLE) break;
    }
    if (_hw->rfalGetTransceiveStatus() != ST_ERR_NONE || rxBuf[0] != 0x0A) {
        ST25R_LOG("_writeMagicGen1UID: 0x40 no ACK (got 0x%02X)", rxBuf[0]);
        return false;
    }

    uint8_t cmd43 = 0x43;
    memset(rxBuf, 0, sizeof(rxBuf));
    actLen = 0;
    ctx.txBuf = &cmd43;
    ctx.txBufLen = 8;
    ctx.rxBuf = rxBuf;
    ctx.rxBufLen = 32;
    ctx.rxRcvdLen = &actLen;
    ctx.flags = (uint32_t)RFAL_TXRX_FLAGS_CRC_TX_MANUAL | (uint32_t)RFAL_TXRX_FLAGS_PAR_TX_NONE |
                (uint32_t)RFAL_TXRX_FLAGS_CRC_RX_KEEP | (uint32_t)RFAL_TXRX_FLAGS_PAR_RX_KEEP;
    ctx.fwt = rfalConv64fcTo1fc(5000);

    if (_hw->rfalStartTransceive(&ctx) != ST_ERR_NONE) return false;
    t0 = millis();
    while (millis() - t0 < 50) {
        _hw->rfalWorker();
        if (_hw->rfalGetTransceiveState() == RFAL_TXRX_STATE_IDLE) break;
    }
    if (_hw->rfalGetTransceiveStatus() != ST_ERR_NONE || rxBuf[0] != 0x0A) {
        ST25R_LOG("_writeMagicGen1UID: 0x43 no ACK (got 0x%02X)", rxBuf[0]);
        return false;
    }

    // Build block 0: UID (4 bytes) + BCC + SAK + ATQA (2) + filler (8)
    uint8_t block0[16] = {0};
    uint8_t bcc = 0;
    int uidLen = uid.size < 4 ? uid.size : 4;
    for (int i = 0; i < uidLen; i++) {
        block0[i] = uid.uidByte[i];
        bcc ^= uid.uidByte[i];
    }
    block0[4] = bcc;
    block0[5] = uid.sak;
    block0[6] = uid.atqaByte[1];
    block0[7] = uid.atqaByte[0];

    // Send MIFARE WRITE block 0 command (with CRC)
    uint8_t writeCmd[2] = {0xA0, 0x00};
    memset(rxBuf, 0, sizeof(rxBuf));
    actLen = 0;
    auto err = _hw->rfalTransceiveBlockingTxRx(
        writeCmd, sizeof(writeCmd), rxBuf, sizeof(rxBuf), &actLen, RFAL_TXRX_FLAGS_DEFAULT, RFAL_FWT_NONE
    );
    if (err != ST_ERR_NONE || rxBuf[0] != 0x0A) {
        ST25R_LOG("_writeMagicGen1UID: WRITE cmd no ACK err=%d", (int)err);
        return false;
    }

    // Send block data (with CRC)
    memset(rxBuf, 0, sizeof(rxBuf));
    actLen = 0;
    err = _hw->rfalTransceiveBlockingTxRx(
        block0, sizeof(block0), rxBuf, sizeof(rxBuf), &actLen, RFAL_TXRX_FLAGS_DEFAULT, RFAL_FWT_NONE
    );
    if (err != ST_ERR_NONE) {
        ST25R_LOG("_writeMagicGen1UID: data send failed err=%d", (int)err);
        return false;
    }

    ST25R_LOG("_writeMagicGen1UID: block 0 written successfully");
    return true;
}

int ST25R3916::write(int cardBaudRate) {
    if (!_nfc) return FAILURE;
    _deselectSharedSpiDevices();

    rfalNfcDevice *dev;
    if (!_pollForTag(&dev)) return TAG_NOT_PRESENT;

    if (dev->type != RFAL_NFC_LISTEN_TYPE_NFCA) {
        _nfc->rfalNfcDeactivate(false);
        return TAG_NOT_MATCH;
    }

    bool storedIsUltralight = printableUID.picc_type.indexOf("Ultralight") >= 0;
    bool presentIsUltralight = (dev->dev.nfca.selRes.sak == 0x00);

    if (!storedIsUltralight || !presentIsUltralight) {
        _nfc->rfalNfcDeactivate(false);
        return TAG_NOT_MATCH;
    }

    return _writeUltralight(dev);
}

int ST25R3916::clone() {
    if (!_nfc) return FAILURE;
    _deselectSharedSpiDevices();

    rfalNfcDevice *dev = nullptr;
    if (!_pollForTag(&dev)) return TAG_NOT_PRESENT;

    if (dev->type != RFAL_NFC_LISTEN_TYPE_NFCA) {
        _nfc->rfalNfcDeactivate(false);
        return TAG_NOT_MATCH;
    }

    if (_writeMagicGen2UID(dev)) {
        _nfc->rfalNfcDeactivate(false);
        return SUCCESS;
    }

    _nfc->rfalNfcDeactivate(false);
    delay(50);

    dev = nullptr;
    if (!_pollForTag(&dev)) return TAG_NOT_PRESENT;

    if (dev->type != RFAL_NFC_LISTEN_TYPE_NFCA) {
        _nfc->rfalNfcDeactivate(false);
        return TAG_NOT_MATCH;
    }

    if (_writeMagicGen1UID(dev)) {
        _nfc->rfalNfcDeactivate(false);
        return SUCCESS;
    }

    _nfc->rfalNfcDeactivate(false);
    return FAILURE;
}

int ST25R3916::erase() {
    if (!_nfc) return FAILURE;
    _deselectSharedSpiDevices();

    rfalNfcDevice *dev;
    if (!_pollForTag(&dev)) return TAG_NOT_PRESENT;

    if (dev->type == RFAL_NFC_LISTEN_TYPE_NFCA && dev->dev.nfca.selRes.sak == 0x00) {
        return _eraseUltralight(dev);
    }

    _nfc->rfalNfcDeactivate(false);
    return NOT_IMPLEMENTED;
}

int ST25R3916::write_ndef() {
    if (!_nfc) return FAILURE;
    _deselectSharedSpiDevices();

    rfalNfcDevice *dev;
    if (!_pollForTag(&dev)) return TAG_NOT_PRESENT;

    if (dev->type != RFAL_NFC_LISTEN_TYPE_NFCA || dev->dev.nfca.selRes.sak != 0x00) {
        _nfc->rfalNfcDeactivate(false);
        return TAG_NOT_MATCH;
    }

    if (totalPages <= 0) {
        _parseDevice(dev);
        if (_readDataBlocks(dev) != SUCCESS) {
            _nfc->rfalNfcDeactivate(false);
            return FAILURE;
        }
    }

    int result = _writeNdefBlocks(dev);
    _nfc->rfalNfcDeactivate(false);
    return result;
}

int ST25R3916::emulate() {
    if (!_hw) return FAILURE;

    std::vector<uint8_t> emulatedNdefMessage;
    if (!_buildLoadedNdefMessage(emulatedNdefMessage)) {
        ST25R_LOG("emulate: no NDEF payload available from loaded/read data");
        return FAILURE;
    }

    ST25R_LOG(
        "emulate: prepared Type 4 NDEF payload from loaded data, ndefLen=%u uid=%s",
        (unsigned)emulatedNdefMessage.size(),
        printableUID.uid.c_str()
    );

    rfalLmConfPA confA;
    memset(&confA, 0, sizeof(confA));
    confA.nfcidLen = uid.size >= 7 ? RFAL_LM_NFCID_LEN_07 : RFAL_LM_NFCID_LEN_04;

    uint8_t nfcid[7] = {0x08, 0x04, 0x25, 0x85, 0x93, 0x10, 0x01};
    if (uid.size >= 4) {
        size_t copyLen = uid.size >= 7 ? 7 : 4;
        memcpy(nfcid, uid.uidByte, copyLen);
    }
    memcpy(confA.nfcid, nfcid, confA.nfcidLen == RFAL_LM_NFCID_LEN_07 ? 7 : 4);

    confA.SENS_RES[0] = uid.atqaByte[1] ? uid.atqaByte[1] : 0x04;
    confA.SENS_RES[1] = uid.atqaByte[0] ? uid.atqaByte[0] : 0x00;
    confA.SEL_RES = 0x20; // ISO-DEP capable Type 4 Tag emulation.

    uint8_t rxBuf[256] = {0};
    uint16_t rxLenBits = 0;
    auto err = _hw->rfalListenStart(
        RFAL_LM_MASK_NFCA, &confA, nullptr, nullptr, rxBuf, sizeof(rxBuf) * 8, &rxLenBits
    );
    if (err != ST_ERR_NONE) {
        ST25R_LOG(
            "emulate: rfalListenStart failed err=%d. ST25R3916-fork currently returns ST_ERR_NOTSUPP for listen mode",
            (int)err
        );
        return (err == ST_ERR_NOTSUPP) ? NOT_IMPLEMENTED : FAILURE;
    }

    _hw->rfalListenStop();
    ST25R_LOG("emulate: listen mode started, but APDU Type 4 exchange is not implemented for this driver yet");
    return NOT_IMPLEMENTED;
}

int ST25R3916::load() {
    FS *fs;
    if (!getFsStorage(fs)) return FAILURE;

    String filepath = loopSD(*fs, true, "RFID|NFC", "/BruceRFID");
    if (filepath.length() == 0) return FAILURE;

    File file = fs->open(filepath, FILE_READ);
    if (!file) return FAILURE;

    String line;
    String strData;
    strAllPages = "";
    totalPages = 0;
    dataPages = 0;
    pageReadSuccess = true;
    pageReadStatus = SUCCESS;

    while (file.available()) {
        line = file.readStringUntil('\n');
        line.trim();
        strData = line.substring(line.indexOf(":") + 1);
        strData.trim();

        if (line.startsWith("Device type:")) printableUID.picc_type = strData;
        else if (line.startsWith("UID:")) printableUID.uid = strData;
        else if (line.startsWith("SAK:")) printableUID.sak = strData;
        else if (line.startsWith("ATQA:")) printableUID.atqa = strData;
        else if (line.startsWith("Pages total:")) totalPages = strData.toInt();
        else if (line.startsWith("Pages read:")) pageReadSuccess = false;
        else if (line.startsWith("Page ")) {
            strAllPages += line + "\n";
            dataPages++;
        }
    }

    file.close();

    if (totalPages == 0) totalPages = dataPages;
    if (printableUID.uid.length() == 0) return FAILURE;

    _parseLoadedData();
    return SUCCESS;
}

int ST25R3916::save(String filename) {
    FS *fs;
    if (!getFsStorage(fs)) return FAILURE;

    File file = createNewFile(fs, "/BruceRFID", filename + ".rfid");
    if (!file) return FAILURE;

    file.println("Filetype: Bruce RFID File");
    file.println("Version 1");
    file.println("Device type: " + printableUID.picc_type);
    file.println("# UID, ATQA and SAK are common for all formats");
    file.println("UID: " + printableUID.uid);
    file.println("SAK: " + printableUID.sak);
    file.println("ATQA: " + printableUID.atqa);
    file.println("# Memory dump");
    file.println("Pages total: " + String(totalPages > 0 ? totalPages : dataPages));
    if (!pageReadSuccess) file.println("Pages read: " + String(dataPages));
    file.print(strAllPages);

    file.close();
    delay(100);
    return SUCCESS;
}

#endif // !LITE_VERSION
