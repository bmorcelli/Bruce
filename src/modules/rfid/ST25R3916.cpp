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
    params.techs2Find =
        RFAL_NFC_POLL_TECH_A | RFAL_NFC_POLL_TECH_B | RFAL_NFC_POLL_TECH_V | RFAL_NFC_POLL_TECH_F;
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
            // ATQA em ordem de transmissão (byte baixo primeiro), igual ao PN532/Flipper
            // (ex.: NTAG => "44 00").
            char atqaBuf[6];
            sprintf(atqaBuf, "%02X %02X", uid.atqaByte[0], uid.atqaByte[1]);
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

    // ISO-DEP (ISO14443-4) — T4T / DESFire / EMV (NFC-A SAK=0x20 ou NFC-B).
    if (dev->rfInterface == RFAL_NFC_INTERFACE_ISODEP) { return _readIsoDep(dev); }

    switch (dev->type) {
        case RFAL_NFC_LISTEN_TYPE_NFCB:
            _parseNfcB(dev);
            pageReadStatus = SUCCESS;
            pageReadSuccess = true;
            return SUCCESS;
        case RFAL_NFC_LISTEN_TYPE_NFCV: return _readNfcV(dev);
        case RFAL_NFC_LISTEN_TYPE_NFCF: return _readFeliCa(dev);
        default: break;
    }

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
    if (_ntagPagesHint > 0) {
        // GET_VERSION is authoritative (also covers blank/unformatted tags)
        totalPages = _ntagPagesHint;
    } else {
        switch (ccBuf[2]) {
            case 0x12: totalPages = 45; break;  // NTAG213
            case 0x3E: totalPages = 135; break; // NTAG215
            case 0x6D: totalPages = 231; break; // NTAG216
            default: totalPages = 64; break;    // MF Ultralight
        }
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
        // ATQA em ordem de transmissão (byte baixo primeiro), igual ao PN532/Flipper.
        uid.atqaByte[0] = strtoul(strAtqa.substring(0, 2).c_str(), NULL, 16);
        uid.atqaByte[1] = strtoul(strAtqa.substring(2, 4).c_str(), NULL, 16);
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
    ntagVariant = "";
    ntagHasVersion = false;
    ntagHasSignature = false;
    ntagHasCounters = false;
    _ntagPagesHint = 0;
    bool isNfcaT2T = (dev->type == RFAL_NFC_LISTEN_TYPE_NFCA && uid.sak == 0x00);
    if (isNfcaT2T) {
        // GET_VERSION first: identifies the variant and the authoritative page count
        String variant = _getNtagVariant();
        if (variant.length() > 0) {
            ntagVariant = variant;
            printableUID.picc_type = variant;
        }
    }

    int blkResult = _readDataBlocks(dev);
    ST25R_LOG("_readDataBlocks -> %d pages=%d", blkResult, dataPages);

    if (isNfcaT2T) {
        _readNtagSignature();
        _readNtagCounters();
    }

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

        if (!_writeT2TPage((uint8_t)page, data)) { return FAILURE; }
        pagesWritten++;
    }

    uint8_t zeros[4] = {0x00, 0x00, 0x00, 0x00};
    for (int page = 4 + pagesWritten; _isUltralightUserPage(page); page++) {
        if (!_writeT2TPage((uint8_t)page, zeros)) { return FAILURE; }
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

// ---------------------------------------------------------------------------
// Emulação de tag NFC-A (modo passive target / listen)
//
// O wrapper RFAL de alto nível (rfalListenStart) é stub (ST_ERR_NOTSUPP) neste
// fork. Porém o ST25R3916 faz a anti-colisão NFC-A em hardware no modo
// "Passive Target" (MODE.targ + om_targ_nfca): basta carregar UID/ATQA/SAK na
// PT Memory e o chip responde sozinho SENS_RES/anticolisão/SELECT. A sequência
// de registradores abaixo segue o furi_hal_nfc do Flipper (mesmo CI).
// ---------------------------------------------------------------------------

// Converte strAllPages ("Page N: XX XX XX XX") em _emuPages. Retorna nº de páginas.
int ST25R3916::_buildEmuPages() {
    _emuPageCount = 0;
    memset(_emuPages, 0, sizeof(_emuPages));
    int pos = 0;
    while (pos < strAllPages.length() && _emuPageCount < 256) {
        int nl = strAllPages.indexOf('\n', pos);
        if (nl < 0) nl = strAllPages.length();
        String line = strAllPages.substring(pos, nl);
        line.trim();
        pos = nl + 1;
        if (!line.startsWith("Page ")) continue;
        int colon = line.indexOf(':');
        if (colon < 0) continue;
        int page = line.substring(5, colon).toInt();
        if (page < 0 || page >= 256) continue;
        std::vector<uint8_t> bytes;
        if (!st25ParseHexBytesAfterColon(line, bytes) || bytes.size() < 4) continue;
        memcpy(_emuPages[page], bytes.data(), 4);
        if (page + 1 > _emuPageCount) _emuPageCount = page + 1;
    }
    return _emuPageCount;
}

bool ST25R3916::_setupListenMode(const uint8_t *uidBuf, uint8_t uidLen, const uint8_t *atqa, uint8_t sak) {
    _deselectSharedSpiDevices();
    _hw->st25r3916OscOn();

    // Configura modo + front-end analógico para listen NFC-A (caminho testado do fork).
    auto mres = _hw->rfalSetMode(RFAL_MODE_LISTEN_NFCA, RFAL_BR_106, RFAL_BR_106);
    if (mres != ST_ERR_NONE) {
        ST25R_LOG("emulate: rfalSetMode(LISTEN_NFCA) -> %d", (int)mres);
        return false;
    }

    // Passive target: receptor ligado, detector de campo externo automático (sem TX próprio).
    _hw->st25r3916WriteRegister(
        ST25R3916_REG_OP_CONTROL,
        ST25R3916_REG_OP_CONTROL_en | ST25R3916_REG_OP_CONTROL_rx_en | ST25R3916_REG_OP_CONTROL_en_fd_auto_efd
    );
    _hw->st25r3916WriteRegister(ST25R3916_REG_MODE, ST25R3916_REG_MODE_targ_targ | ST25R3916_REG_MODE_om0);
    _hw->st25r3916WriteRegister(
        ST25R3916_REG_PASSIVE_TARGET,
        ST25R3916_REG_PASSIVE_TARGET_fdel_2 | ST25R3916_REG_PASSIVE_TARGET_fdel_0 |
            ST25R3916_REG_PASSIVE_TARGET_d_ac_ap2p | ST25R3916_REG_PASSIVE_TARGET_d_212_424_1r
    );
    _hw->st25r3916WriteRegister(ST25R3916_REG_MASK_RX_TIMER, 0x02);
    _hw->st25r3916ExecuteCommand(ST25R3916_CMD_STOP);

    // Habilita as interrupções relevantes ao modo target.
    uint32_t interrupts = ST25R3916_IRQ_MASK_FWL | ST25R3916_IRQ_MASK_TXE | ST25R3916_IRQ_MASK_RXS |
                          ST25R3916_IRQ_MASK_RXE | ST25R3916_IRQ_MASK_PAR | ST25R3916_IRQ_MASK_CRC |
                          ST25R3916_IRQ_MASK_ERR1 | ST25R3916_IRQ_MASK_ERR2 | ST25R3916_IRQ_MASK_NRE |
                          ST25R3916_IRQ_MASK_EON | ST25R3916_IRQ_MASK_EOF | ST25R3916_IRQ_MASK_WU_A_X |
                          ST25R3916_IRQ_MASK_WU_A;
    _hw->st25r3916ClearInterrupts();
    _hw->st25r3916DisableInterrupts(ST25R3916_IRQ_MASK_ALL);
    _hw->st25r3916EnableInterrupts(interrupts);

    // UID de 4 ou 7 bytes.
    _hw->st25r3916ChangeRegisterBits(
        ST25R3916_REG_AUX,
        ST25R3916_REG_AUX_nfc_id_mask,
        (uidLen == 4) ? ST25R3916_REG_AUX_nfc_id_4bytes : ST25R3916_REG_AUX_nfc_id_7bytes
    );

    // PT Memory A (15 bytes): UID[0..len], ATQA em [10..11], SAK em [12..14].
    uint8_t pt[ST25R3916_PTM_A_LEN] = {0};
    memcpy(pt, uidBuf, uidLen);
    pt[10] = atqa[0];
    pt[11] = atqa[1];
    pt[12] = (uidLen == 4) ? (uint8_t)(sak & ~0x04) : 0x04; // cascade level 1
    pt[13] = (uint8_t)(sak & ~0x04);
    pt[14] = (uint8_t)(sak & ~0x04);
    _hw->st25r3916WritePTMem(pt, sizeof(pt));

    // Habilita anti-colisão automática (bit limpo) e entra no estado Sense.
    _hw->st25r3916ClrRegisterBits(ST25R3916_REG_PASSIVE_TARGET, ST25R3916_REG_PASSIVE_TARGET_d_106_ac_a);
    _hw->st25r3916ExecuteCommand(ST25R3916_CMD_GOTO_SENSE);
    return true;
}

void ST25R3916::_listenStop() {
    if (!_hw) return;
    _hw->st25r3916ExecuteCommand(ST25R3916_CMD_STOP);
    _hw->st25r3916DisableInterrupts(ST25R3916_IRQ_MASK_ALL);
    _hw->st25r3916ClearInterrupts();
    // Volta ao modo poller padrão para que rfid read funcione em seguida.
    _hw->rfalSetMode(RFAL_MODE_POLL_NFCA, RFAL_BR_106, RFAL_BR_106);
    _hw->rfalFieldOff();
    _discoveryStarted = false;
}

// Transmite uma resposta (com CRC apêndice automático) via FIFO no modo target.
bool ST25R3916::_listenRespond(const uint8_t *resp, uint16_t len) {
    _hw->st25r3916ExecuteCommand(ST25R3916_CMD_CLEAR_FIFO);
    _hw->st25r3916WriteFifo(resp, len);
    _hw->st25r3916SetNumTxBits((uint16_t)(len * 8U));
    _hw->st25r3916ExecuteCommand(ST25R3916_CMD_TRANSMIT_WITH_CRC);
    uint32_t irqs = _hw->st25r3916WaitForInterruptsTimed(ST25R3916_IRQ_MASK_TXE, 20);
    return (irqs & ST25R3916_IRQ_MASK_TXE) != 0U;
}

// Loop principal: o hardware responde anti-colisão/SELECT sozinho; aqui apenas
// detectamos seleções (WU_A) e respondemos comandos T2T (READ/FAST_READ/GET_VERSION).
int ST25R3916::_handleListenLoop(uint32_t timeoutMs) {
    uint32_t deadline = millis() + timeoutMs;
    int readers = 0;
    uint32_t nRead = 0;
    bool active = false;
    uint8_t fifo[64];
    // Trace diferido: registra a sequência de comandos e imprime só quando o leitor
    // sai do campo, p/ não atrasar o caminho RX->resposta (timing do Switch é estrito).
    uint8_t traceCmd[96];
    uint8_t traceArg[96];
    uint16_t traceN = 0;

    while ((int32_t)(deadline - millis()) > 0) {
        vTaskDelay(pdMS_TO_TICKS(1)); // time to pull EscPress from othe tasks
        if (check(EscPress)) {
            ST25R_LOG("emulate: Esc — encerrando");
            break;
        }

        uint32_t irqs = _hw->st25r3916WaitForInterruptsTimed(
            ST25R3916_IRQ_MASK_WU_A | ST25R3916_IRQ_MASK_WU_A_X | ST25R3916_IRQ_MASK_RXE |
                ST25R3916_IRQ_MASK_EOF,
            200
        );
        if (irqs == 0U) continue;

        if ((irqs & (ST25R3916_IRQ_MASK_WU_A | ST25R3916_IRQ_MASK_WU_A_X)) != 0U) {
            if (!active) {
                readers++;
                traceN = 0;
                ST25R_LOG("emulate: reader selecionou a tag (UID enviado) #%d", readers);
            }
            active = true;
            // Passa a tratar dados manualmente (desabilita auto-AC).
            _hw->st25r3916SetRegisterBits(
                ST25R3916_REG_PASSIVE_TARGET, ST25R3916_REG_PASSIVE_TARGET_d_106_ac_a
            );
        }

        if ((irqs & ST25R3916_IRQ_MASK_RXE) != 0U) {
            uint16_t n = _hw->st25r3916GetNumFIFOBytes();
            if (n > 0U && n <= sizeof(fifo)) {
                _hw->st25r3916ReadFifo(fifo, n);
                uint8_t cmd = fifo[0];
                // Registra no trace (impressão diferida p/ não atrasar a resposta).
                if (cmd == 0x30 || cmd == 0x3A) nRead++;
                if (traceN < sizeof(traceCmd)) {
                    traceCmd[traceN] = cmd;
                    traceArg[traceN] = (n > 1) ? fifo[1] : 0;
                    traceN++;
                }
                if (cmd == 0x30 && n >= 2) { // READ: 16 bytes (4 páginas a partir de addr)
                    uint8_t addr = fifo[1];
                    uint8_t resp[16];
                    for (int i = 0; i < 4; i++) {
                        int p = (_emuPageCount > 0) ? ((addr + i) % _emuPageCount) : (addr + i);
                        memcpy(&resp[i * 4], _emuPages[p & 0xFF], 4);
                    }
                    _listenRespond(resp, sizeof(resp));
                } else if (cmd == 0x3A && n >= 3) { // FAST_READ start..end (até 64 páginas/256 B)
                    uint8_t start = fifo[1], end = fifo[2];
                    if (end >= start && (uint16_t)((end - start + 1) * 4) <= 256) {
                        uint8_t resp[256];
                        uint16_t rl = 0;
                        for (int p = start; p <= end; p++) {
                            int pp = (_emuPageCount > 0) ? (p % _emuPageCount) : p;
                            memcpy(&resp[rl], _emuPages[pp & 0xFF], 4);
                            rl += 4;
                        }
                        _listenRespond(resp, rl);
                    }
                } else if (cmd == 0x60) { // GET_VERSION (necessário p/ NTAG21x e amiibo)
                    uint8_t ver[8];
                    if (ntagHasVersion) {
                        memcpy(ver, ntagVersion, 8);
                    } else {
                        // Default NTAG21x conforme nº de páginas (0F=213, 11=215, 13=216).
                        uint8_t storage = 0x11; // NTAG215 (caso típico amiibo)
                        if (_emuPageCount > 0 && _emuPageCount <= 45) storage = 0x0F;
                        else if (_emuPageCount > 135) storage = 0x13;
                        uint8_t def[8] = {0x00, 0x04, 0x04, 0x02, 0x01, 0x00, storage, 0x03};
                        memcpy(ver, def, 8);
                    }
                    _listenRespond(ver, 8);
                } else if (cmd == 0x3C && n >= 2) { // READ_SIG: assinatura ECC (amiibo/Switch)
                    uint8_t sig[32];
                    if (ntagHasSignature) memcpy(sig, ntagSignature, 32);
                    else memset(sig, 0, 32);
                    _listenRespond(sig, 32);
                } else if (cmd == 0x39 && n >= 2) { // READ_CNT: contador de 24 bits
                    uint8_t idx = fifo[1];
                    uint8_t cnt[3] = {0, 0, 0};
                    if (idx < 3 && ntagHasCounters) {
                        cnt[0] = (uint8_t)(ntagCounters[idx] & 0xFF);
                        cnt[1] = (uint8_t)((ntagCounters[idx] >> 8) & 0xFF);
                        cnt[2] = (uint8_t)((ntagCounters[idx] >> 16) & 0xFF);
                    }
                    _listenRespond(cnt, 3);
                } else if (cmd == 0x1B && n >= 5) { // PWD_AUTH: responde PACK
                    uint8_t pack[2] = {0, 0};
                    if (_emuPageCount > 0) {
                        pack[0] = _emuPages[_emuPageCount - 1][0];
                        pack[1] = _emuPages[_emuPageCount - 1][1];
                    }
                    // Amiibo (NTAG215): a senha deriva do UID e o PACK correto é 80 80.
                    // Na tag genuína a página PACK é lida como 00 00 (protegida), então
                    // sem isto o Switch recebe PACK errado e recusa antes de ler os dados.
                    if (uid.size == 7) {
                        uint8_t apwd[4] = {
                            (uint8_t)(0xAA ^ uid.uidByte[1] ^ uid.uidByte[3]),
                            (uint8_t)(0x55 ^ uid.uidByte[2] ^ uid.uidByte[4]),
                            (uint8_t)(0xAA ^ uid.uidByte[3] ^ uid.uidByte[5]),
                            (uint8_t)(0x55 ^ uid.uidByte[4] ^ uid.uidByte[6]),
                        };
                        if (fifo[1] == apwd[0] && fifo[2] == apwd[1] && fifo[3] == apwd[2] &&
                            fifo[4] == apwd[3]) {
                            pack[0] = 0x80;
                            pack[1] = 0x80;
                        }
                    }
                    _listenRespond(pack, 2);
                } else if (cmd == 0x50) { // HALT
                    _hw->st25r3916ExecuteCommand(ST25R3916_CMD_GOTO_SLEEP);
                } else if (cmd == 0xA2 && n >= 6) { // WRITE: aceita e guarda, ACK
                    uint8_t addr = fifo[1];
                    if (addr < 256) {
                        memcpy(_emuPages[addr], &fifo[2], 4);
                        if (addr + 1 > _emuPageCount) _emuPageCount = addr + 1;
                    }
                    uint8_t ack = 0x0A; // ACK NTAG (4 bits, sem CRC)
                    _hw->st25r3916ExecuteCommand(ST25R3916_CMD_CLEAR_FIFO);
                    _hw->st25r3916WriteFifo(&ack, 1);
                    _hw->st25r3916SetNumTxBits(4);
                    _hw->st25r3916ExecuteCommand(ST25R3916_CMD_TRANSMIT_WITHOUT_CRC);
                }
            }
        }

        if ((irqs & ST25R3916_IRQ_MASK_EOF) != 0U) {
            // Campo do reader desligou: imprime o trace acumulado e rearma.
            if (active) {
                String tr;
                for (uint16_t i = 0; i < traceN; i++) {
                    char b[8];
                    if (traceCmd[i] == 0x30 || traceCmd[i] == 0x3A)
                        sprintf(b, "%02X:%02X ", traceCmd[i], traceArg[i]);
                    else sprintf(b, "%02X ", traceCmd[i]);
                    tr += b;
                }
                ST25R_LOG(
                    "emulate: reader saiu (cmds=%u reads=%lu): %s", traceN, (unsigned long)nRead, tr.c_str()
                );
            }
            active = false;
            _hw->st25r3916ClrRegisterBits(
                ST25R3916_REG_PASSIVE_TARGET, ST25R3916_REG_PASSIVE_TARGET_d_106_ac_a
            );
            _hw->st25r3916ExecuteCommand(ST25R3916_CMD_GOTO_SENSE);
        }
    }
    return readers;
}

int ST25R3916::emulate() {
    if (!_hw || !_nfc) return FAILURE;

    // Obtém UID: do struct (após read) ou do printableUID (após load).
    uint8_t uidBuf[7] = {0};
    uint8_t uidLen = 0;
    if (uid.size == 4 || uid.size == 7) {
        uidLen = uid.size;
        memcpy(uidBuf, uid.uidByte, uidLen);
    } else if (printableUID.uid.length() > 0) {
        std::vector<uint8_t> ub;
        int hi = -1;
        for (unsigned i = 0; i < printableUID.uid.length(); i++) {
            int v = st25HexNibble(printableUID.uid.charAt(i));
            if (v < 0) continue;
            if (hi < 0) hi = v;
            else {
                ub.push_back((uint8_t)((hi << 4) | v));
                hi = -1;
            }
        }
        if (ub.size() == 4 || ub.size() == 7) {
            uidLen = ub.size();
            memcpy(uidBuf, ub.data(), uidLen);
        }
    }
    if (uidLen != 4 && uidLen != 7) {
        ST25R_LOG("emulate: UID inválido/ausente — faça 'rfid read' ou 'rfid loadfile' antes");
        return FAILURE;
    }

    uint8_t atqa[2];
    atqa[0] = uid.atqaByte[0] ? uid.atqaByte[0] : (uidLen == 7 ? 0x44 : 0x04);
    atqa[1] = uid.atqaByte[1];
    uint8_t sak = uid.sak; // 0x00 = NTAG/Ultralight (T2T)

    _buildEmuPages();

    // Garante que nenhum discovery/poller esteja ativo antes de virar target.
    stopDiscovery();
    _nfc->rfalNfcDeactivate(false);
    _discoveryStarted = false;

    if (!_setupListenMode(uidBuf, uidLen, atqa, sak)) {
        _listenStop();
        return FAILURE;
    }

    ST25R_LOG(
        "emulate: emulando UID=%s ATQA=%02X%02X SAK=%02X pages=%d ver=%d sig=%d (timeout 30s, Esc p/ sair)",
        printableUID.uid.c_str(),
        atqa[1],
        atqa[0],
        sak,
        _emuPageCount,
        ntagHasVersion ? 1 : 0,
        ntagHasSignature ? 1 : 0
    );

    int readers = _handleListenLoop(30000);

    _listenStop();
    ST25R_LOG("emulate: encerrado — readers detectados=%d", readers);
    return SUCCESS;
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
    ntagHasVersion = false;
    ntagHasSignature = false;
    ntagHasCounters = false;

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
        // NTAG/Ultralight: restaura versão/assinatura/contadores p/ emulação fiel.
        else if (line.startsWith("Mifare version:")) {
            std::vector<uint8_t> b;
            if (st25ParseHexBytesAfterColon(line, b) && b.size() >= 8) {
                memcpy(ntagVersion, b.data(), 8);
                ntagHasVersion = true;
            }
        } else if (line.startsWith("Signature:")) {
            std::vector<uint8_t> b;
            if (st25ParseHexBytesAfterColon(line, b) && b.size() >= 32) {
                memcpy(ntagSignature, b.data(), 32);
                ntagHasSignature = true;
            }
        } else if (line.startsWith("Counter ")) {
            int idx = line.substring(8, line.indexOf(":")).toInt();
            if (idx >= 0 && idx < 3) {
                ntagCounters[idx] = (uint32_t)strData.toInt();
                ntagHasCounters = true;
            }
        } else if (line.startsWith("Page ")) {
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

String ST25R3916::_getNtagVariant() {
    ntagHasVersion = false;
    memset(ntagVersion, 0, sizeof(ntagVersion));

    uint8_t cmd = 0x60;
    uint8_t rx[10] = {0};
    uint16_t rxLen = 0;
    auto err = _hw->rfalTransceiveBlockingTxRx(
        &cmd, 1, rx, sizeof(rx), &rxLen, RFAL_TXRX_FLAGS_DEFAULT, rfalConvMsTo1fc(20)
    );
    if (err != ST_ERR_NONE || rxLen < 8) {
        ST25R_LOG("_getNtagVariant: GET_VERSION failed err=%d len=%u", (int)err, rxLen);
        return "";
    }
    memcpy(ntagVersion, rx, 8);
    ntagHasVersion = true;
    ST25R_LOG(
        "version=%02X %02X %02X %02X %02X %02X %02X %02X",
        rx[0],
        rx[1],
        rx[2],
        rx[3],
        rx[4],
        rx[5],
        rx[6],
        rx[7]
    );

    uint8_t productType = rx[2]; // 0x03 = Ultralight, 0x04 = NTAG
    uint8_t storage = rx[6];     // storage size code
    if (productType == 0x04) {
        switch (storage) {
            case 0x0F: _ntagPagesHint = 45; return "NTAG213";
            case 0x11: _ntagPagesHint = 135; return "NTAG215";
            case 0x13: _ntagPagesHint = 231; return "NTAG216";
            default: return "NTAG21x";
        }
    }
    if (productType == 0x03) {
        switch (storage) {
            case 0x0B: _ntagPagesHint = 20; return "MF Ultralight EV1 (UL11)";
            case 0x0E: _ntagPagesHint = 41; return "MF Ultralight EV1 (UL21)";
            default: return "MF Ultralight EV1";
        }
    }
    return "";
}

bool ST25R3916::_readNtagSignature() {
    ntagHasSignature = false;
    memset(ntagSignature, 0, sizeof(ntagSignature));

    uint8_t cmd[2] = {0x3C, 0x00};
    uint8_t rx[40] = {0};
    uint16_t rxLen = 0;
    auto err = _hw->rfalTransceiveBlockingTxRx(
        cmd, sizeof(cmd), rx, sizeof(rx), &rxLen, RFAL_TXRX_FLAGS_DEFAULT, rfalConvMsTo1fc(20)
    );
    if (err != ST_ERR_NONE || rxLen < 32) {
        ST25R_LOG("_readNtagSignature: READ_SIG failed err=%d len=%u", (int)err, rxLen);
        return false;
    }
    memcpy(ntagSignature, rx, 32);
    ntagHasSignature = true;
    ST25R_LOG("signature=%02X %02X %02X %02X...%02X %02X", rx[0], rx[1], rx[2], rx[3], rx[30], rx[31]);
    return true;
}

bool ST25R3916::_readNtagCounters() {
    ntagHasCounters = false;
    bool any = false;
    for (uint8_t idx = 0; idx < 3; idx++) {
        ntagCounters[idx] = 0;
        ntagTearing[idx] = 0xBD;

        uint8_t cmd[2] = {0x39, idx};
        uint8_t rx[4] = {0};
        uint16_t rxLen = 0;
        auto err = _hw->rfalTransceiveBlockingTxRx(
            cmd, sizeof(cmd), rx, sizeof(rx), &rxLen, RFAL_TXRX_FLAGS_DEFAULT, rfalConvMsTo1fc(20)
        );
        if (err == ST_ERR_NONE && rxLen >= 3) {
            ntagCounters[idx] = (uint32_t)rx[0] | ((uint32_t)rx[1] << 8) | ((uint32_t)rx[2] << 16);
            any = true;
            ST25R_LOG("counter[%u]=%lu", idx, (unsigned long)ntagCounters[idx]);
        }
    }
    ntagHasCounters = any;
    return any;
}

static String _bytesToHex(const uint8_t *data, size_t len);

int ST25R3916::_readNfcV(rfalNfcDevice *dev) {
    strAllPages = "";
    dataPages = 0;
    totalPages = 0;

    const uint8_t *nfcvUid = dev->nfcid;
    uint8_t flags = RFAL_NFCV_REQ_FLAG_DEFAULT;

    uint8_t blockCount = 0;
    uint8_t blockSize = 4;
    {
        uint8_t sysInfo[32] = {0};
        uint16_t rcvLen = 0;
        auto err =
            _nfc->rfalNfcvPollerGetSystemInformation(flags, nfcvUid, sysInfo, sizeof(sysInfo), &rcvLen);
        if (err == ST_ERR_NONE && rcvLen >= 10) {
            // Layout ISO15693: RES_FLAG(1) InfoFlags(1) UID(8) [DSFID] [AFI] [MemSize(2)] [ICref]
            uint8_t infoFlags = sysInfo[1];
            size_t idx = 10;
            if (infoFlags & 0x01) idx += 1; // DSFID presente
            if (infoFlags & 0x02) idx += 1; // AFI presente
            if ((infoFlags & 0x04) && (idx + 1 < rcvLen)) {
                blockCount = sysInfo[idx] + 1;             // nº de blocos (valor+1)
                blockSize = (sysInfo[idx + 1] & 0x1F) + 1; // tamanho do bloco (valor+1)
            }
            ST25R_LOG("NFC-V SysInfo flags=0x%02X blocks=%u size=%u", infoFlags, blockCount, blockSize);
        } else {
            ST25R_LOG("NFC-V GetSystemInformation err=%d len=%u (lendo até falhar)", (int)err, rcvLen);
        }
    }

    if (blockSize == 0 || blockSize > 8) blockSize = 4;
    uint8_t maxBlocks = (blockCount > 0) ? blockCount : 255;

    for (uint16_t blk = 0; blk < maxBlocks; blk++) {
        uint8_t rxBuf[16] = {0};
        uint16_t rxLen = 0;
        auto err =
            _nfc->rfalNfcvPollerReadSingleBlock(flags, nfcvUid, (uint8_t)blk, rxBuf, sizeof(rxBuf), &rxLen);
        if (err != ST_ERR_NONE || rxLen < (uint16_t)(blockSize + 1)) {
            if (blockCount == 0) break; // tamanho desconhecido: parar no 1º erro
            // tamanho conhecido: bloco ilegível, registra como zeros e segue
            char line[64];
            sprintf(line, "Block %02u: (unreadable err=%d)", blk, (int)err);
            strAllPages += String(line) + "\n";
            continue;
        }
        // rxBuf[0] = RES_FLAG; dados a partir de rxBuf[1]
        String line = "Block ";
        char idxBuf[8];
        sprintf(idxBuf, "%02u: ", blk);
        line += idxBuf;
        for (uint8_t b = 0; b < blockSize; b++) {
            char hb[4];
            sprintf(hb, "%02X", rxBuf[1 + b]);
            line += hb;
            if (b < blockSize - 1) line += " ";
        }
        strAllPages += line + "\n";
        dataPages++;
    }

    totalPages = (blockCount > 0) ? blockCount : dataPages;
    ST25R_LOG("NFC-V read: %d/%d blocos lidos", dataPages, totalPages);

    if (dataPages == 0) {
        pageReadStatus = FAILURE;
        return FAILURE;
    }
    pageReadStatus = SUCCESS;
    pageReadSuccess = true;
    return SUCCESS;
}

void ST25R3916::_parseNfcB(rfalNfcDevice *dev) {
    rfalNfcbListenDevice *nfcb = &dev->dev.nfcb;
    const uint8_t *pupi = nfcb->sensbRes.nfcid0; // 4 bytes (PUPI / pseudo-UID)

    printableUID.picc_type = "ISO14443B";
    printableUID.sak = "--";
    printableUID.atqa = "--";

    printableUID.uid = "";
    for (int i = 0; i < RFAL_NFCB_NFCID0_LEN; i++) {
        char buf[3];
        sprintf(buf, "%02X", pupi[i]);
        printableUID.uid += buf;
        if (i < RFAL_NFCB_NFCID0_LEN - 1) printableUID.uid += " ";
    }

    const uint8_t *appData = (const uint8_t *)&nfcb->sensbRes.appData;
    const uint8_t *protInfo = (const uint8_t *)&nfcb->sensbRes.protInfo;

    strAllPages = "";
    strAllPages += "PUPI: " + printableUID.uid + "\n";
    strAllPages += "Application data: " + _bytesToHex(appData, 4) + "\n";
    strAllPages += "Protocol info: " + _bytesToHex(protInfo, 4) + "\n";

    ST25R_LOG(
        "NFC-B PUPI=%s appData=%02X%02X%02X%02X protInfo=%02X%02X%02X%02X",
        printableUID.uid.c_str(),
        appData[0],
        appData[1],
        appData[2],
        appData[3],
        protInfo[0],
        protInfo[1],
        protInfo[2],
        protInfo[3]
    );
}

int ST25R3916::_readFeliCa(rfalNfcDevice *dev) {
    rfalNfcfListenDevice *nfcf = &dev->dev.nfcf;
    const uint8_t *idm = nfcf->sensfRes.NFCID2;                // 8 bytes (IDm)
    const uint8_t *pmm = (const uint8_t *)nfcf->sensfRes.PAD0; // 8 bytes contíguos (PMm)

    printableUID.picc_type = "FeliCa";
    printableUID.sak = "--";
    printableUID.atqa = "--";

    printableUID.uid = "";
    for (int i = 0; i < RFAL_NFCF_NFCID2_LEN; i++) {
        char buf[3];
        sprintf(buf, "%02X", idm[i]);
        printableUID.uid += buf;
        if (i < RFAL_NFCF_NFCID2_LEN - 1) printableUID.uid += " ";
    }

    strAllPages = "";
    strAllPages += "IDm: " + _bytesToHex(idm, 8) + "\n";
    strAllPages += "PMm: " + _bytesToHex(pmm, 8) + "\n";

    ST25R_LOG("FeliCa IDm=%s PMm=%s", _bytesToHex(idm, 8).c_str(), _bytesToHex(pmm, 8).c_str());

    rfalNfcfServ service = 0x000B;
    rfalNfcfBlockListElem block;
    block.conf = 0x80; // 2-byte block list element, acesso normal
    block.blockNum = 0;
    rfalNfcfServBlockListParam servBlock;
    servBlock.numServ = 1;
    servBlock.servList = &service;
    servBlock.numBlock = 1;
    servBlock.blockList = &block;

    uint8_t rxBuf[32] = {0};
    uint16_t rcvdLen = 0;
    auto err = _nfc->rfalNfcfPollerCheck(idm, &servBlock, rxBuf, sizeof(rxBuf), &rcvdLen);
    if (err == ST_ERR_NONE && rcvdLen >= 16) {
        // resposta T3T Check: status flags + dados do bloco (16 bytes)
        strAllPages += "Block 00: " + _bytesToHex(rxBuf + (rcvdLen - 16), 16) + "\n";
        ST25R_LOG("FeliCa block0 lido (%u bytes)", rcvdLen);
        dataPages = 1;
    } else {
        ST25R_LOG("FeliCa Check err=%d len=%u (serviço pode exigir chave)", (int)err, rcvdLen);
        dataPages = 0;
    }

    totalPages = dataPages;
    pageReadStatus = SUCCESS;
    pageReadSuccess = true;
    return SUCCESS;
}

// ============================================================================
// ISO-DEP / Type 4 Tag (T4T): DESFire, NDEF T4T, EMV
// ============================================================================

bool ST25R3916::_isoDepApdu(const uint8_t *tx, uint16_t txLen, uint8_t *rx, uint16_t rxCap, uint16_t *rxLen) {
    if (rxLen) *rxLen = 0;
    uint8_t *rxData = nullptr;
    uint16_t *rcvLen = nullptr;
    auto err =
        _nfc->rfalNfcDataExchangeStart(const_cast<uint8_t *>(tx), txLen, &rxData, &rcvLen, RFAL_FWT_NONE);
    if (err != ST_ERR_NONE) {
        ST25R_LOG("isoDep APDU start err=%d", (int)err);
        return false;
    }

    uint16_t total = 0;
    uint32_t t0 = millis();
    for (;;) {
        _nfc->rfalNfcWorker();
        err = _nfc->rfalNfcDataExchangeGetStatus();
        if (err == ST_ERR_BUSY) {
            if (millis() - t0 > 2000) {
                ST25R_LOG("isoDep APDU timeout");
                return false;
            }
            continue;
        }
        // Copia o bloco recebido (NONE = último; AGAIN = chaining, virão mais).
        uint16_t n = (rcvLen != nullptr) ? *rcvLen : 0;
        if (rxData != nullptr && rx != nullptr && n > 0 && total < rxCap) {
            uint16_t cp = (total + n > rxCap) ? (uint16_t)(rxCap - total) : n;
            memcpy(rx + total, rxData, cp);
            total += cp;
        }
        if (err == ST_ERR_AGAIN) {
            t0 = millis();
            continue;
        }
        if (err != ST_ERR_NONE) {
            ST25R_LOG("isoDep APDU status err=%d", (int)err);
            return false;
        }
        break;
    }
    if (rxLen) *rxLen = total;
    return true;
}

// Verifica se a resposta termina com Status Word 0x9000.
static bool _swOk(const uint8_t *rx, uint16_t len) {
    return (len >= 2 && rx[len - 2] == 0x90 && rx[len - 1] == 0x00);
}

// NDEF Type 4 Tag: SELECT NDEF App -> SELECT CC -> READ CC -> SELECT NDEF file -> READ.
bool ST25R3916::_readNdefT4T() {
    uint8_t rx[256];
    uint16_t rxLen = 0;

    // 1. SELECT NDEF Tag Application (AID D2 76 00 00 85 01 01)
    static const uint8_t selApp[] = {
        0x00, 0xA4, 0x04, 0x00, 0x07, 0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01, 0x00
    };
    if (!_isoDepApdu(selApp, sizeof(selApp), rx, sizeof(rx), &rxLen) || !_swOk(rx, rxLen)) {
        ST25R_LOG("T4T: SELECT NDEF App falhou");
        return false;
    }

    // 2. SELECT Capability Container (file ID E1 03)
    static const uint8_t selCC[] = {0x00, 0xA4, 0x00, 0x0C, 0x02, 0xE1, 0x03};
    if (!_isoDepApdu(selCC, sizeof(selCC), rx, sizeof(rx), &rxLen) || !_swOk(rx, rxLen)) return false;

    // 3. READ BINARY do CC (15 bytes)
    static const uint8_t readCC[] = {0x00, 0xB0, 0x00, 0x00, 0x0F};
    if (!_isoDepApdu(readCC, sizeof(readCC), rx, sizeof(rx), &rxLen) || !_swOk(rx, rxLen) || rxLen < 15 + 2) {
        return false;
    }
    // CC: [7]=NDEF FileCtrl TLV tag(0x04) [8]=len(0x06) [9-10]=NDEF file ID [11-12]=max NDEF size
    uint16_t ndefFileId = ((uint16_t)rx[9] << 8) | rx[10];
    uint16_t maxNdef = ((uint16_t)rx[11] << 8) | rx[12];

    // 4. SELECT NDEF file
    uint8_t selNdef[] = {
        0x00, 0xA4, 0x00, 0x0C, 0x02, (uint8_t)(ndefFileId >> 8), (uint8_t)(ndefFileId & 0xFF)
    };
    if (!_isoDepApdu(selNdef, sizeof(selNdef), rx, sizeof(rx), &rxLen) || !_swOk(rx, rxLen)) return false;

    // 5. READ NLEN (2 bytes no offset 0)
    static const uint8_t readNlen[] = {0x00, 0xB0, 0x00, 0x00, 0x02};
    if (!_isoDepApdu(readNlen, sizeof(readNlen), rx, sizeof(rx), &rxLen) || rxLen < 4) return false;
    uint16_t ndefLen = ((uint16_t)rx[0] << 8) | rx[1];

    printableUID.picc_type = "NDEF T4T";
    strAllPages = "";
    strAllPages += "NDEF T4T\n";
    char fidLine[40];
    sprintf(fidLine, "NDEF file: %04X (max %u)\n", ndefFileId, maxNdef);
    strAllPages += fidLine;
    strAllPages += "NDEF len: " + String(ndefLen) + "\n";

    // 6. READ BINARY da mensagem NDEF (offset 2). Limita a um único READ.
    if (ndefLen > 0) {
        uint16_t toRead = (ndefLen > 240) ? 240 : ndefLen;
        uint8_t readMsg[] = {0x00, 0xB0, 0x00, 0x02, (uint8_t)toRead};
        if (_isoDepApdu(readMsg, sizeof(readMsg), rx, sizeof(rx), &rxLen) && rxLen >= toRead) {
            strAllPages += "NDEF: " + _bytesToHex(rx, toRead) + "\n";
        }
    }
    dataPages = 1;
    pageReadSuccess = true;
    pageReadStatus = SUCCESS;
    return true;
}

// MIFARE DESFire: GetVersion (0x60) e lista de aplicações (0x6A), via APDU wrapped.
bool ST25R3916::_readDESFireInfo() {
    uint8_t rx[64];
    uint16_t rxLen = 0;

    static const uint8_t getVer[] = {0x90, 0x60, 0x00, 0x00, 0x00};
    if (!_isoDepApdu(getVer, sizeof(getVer), rx, sizeof(rx), &rxLen) || rxLen < 2) return false;
    // DESFire responde SW1=0x91 com SW2 0xAF (additional frame) ou 0x00.
    if (rx[rxLen - 2] != 0x91) {
        ST25R_LOG("DESFire GetVersion SW=%02X%02X (não-DESFire)", rx[rxLen - 2], rx[rxLen - 1]);
        return false;
    }

    printableUID.picc_type = "MIFARE DESFire";
    strAllPages = "";
    strAllPages += "MIFARE DESFire\n";
    if (rxLen >= 7 + 2) {
        // HW info: vendor type subtype major minor storage proto
        strAllPages += "HW: " + _bytesToHex(rx, 7) + "\n";
    }

    // Coleta frames adicionais (SW 91 AF) — limite de segurança de 3 frames.
    int guard = 0;
    while (rxLen >= 2 && rx[rxLen - 2] == 0x91 && rx[rxLen - 1] == 0xAF && guard++ < 3) {
        static const uint8_t more[] = {0x90, 0xAF, 0x00, 0x00, 0x00};
        if (!_isoDepApdu(more, sizeof(more), rx, sizeof(rx), &rxLen) || rxLen < 2) break;
        strAllPages += "Frame: " + _bytesToHex(rx, (uint16_t)(rxLen - 2)) + "\n";
    }

    // GetApplicationIDs (0x6A) — lista AIDs de 3 bytes.
    static const uint8_t getApps[] = {0x90, 0x6A, 0x00, 0x00, 0x00};
    if (_isoDepApdu(getApps, sizeof(getApps), rx, sizeof(rx), &rxLen) && rxLen >= 2 &&
        rx[rxLen - 2] == 0x91) {
        uint16_t n = (uint16_t)(rxLen - 2);
        strAllPages += "AIDs: " + (n > 0 ? _bytesToHex(rx, n) : String("none")) + "\n";
    }

    dataPages = 1;
    pageReadSuccess = true;
    pageReadStatus = SUCCESS;
    return true;
}

// Cartão EMV (pagamento): SELECT PPSE e lista os AIDs públicos (tag 4F). Sem dados sensíveis.
bool ST25R3916::_probeEmv() {
    uint8_t rx[256];
    uint16_t rxLen = 0;

    // SELECT PPSE "2PAY.SYS.DDF01"
    static const uint8_t selPpse[] = {0x00, 0xA4, 0x04, 0x00, 0x0E, 0x32, 0x50, 0x41, 0x59, 0x2E,
                                      0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0x00};
    if (!_isoDepApdu(selPpse, sizeof(selPpse), rx, sizeof(rx), &rxLen) || !_swOk(rx, rxLen)) {
        ST25R_LOG("EMV: SELECT PPSE falhou");
        return false;
    }

    printableUID.picc_type = "EMV";
    strAllPages = "";
    strAllPages += "EMV (payment card)\n";
    uint16_t fciLen = (uint16_t)(rxLen - 2);
    strAllPages += "FCI: " + _bytesToHex(rx, fciLen) + "\n";

    // Procura tags 4F (Application Identifier) na resposta FCI.
    bool foundAid = false;
    for (uint16_t i = 0; i + 1 < fciLen; i++) {
        if (rx[i] == 0x4F) {
            uint8_t len = rx[i + 1];
            if (len >= 5 && len <= 16 && (uint16_t)(i + 2 + len) <= fciLen) {
                strAllPages += "AID: " + _bytesToHex(rx + i + 2, len) + "\n";
                foundAid = true;
            }
        }
    }
    if (!foundAid) strAllPages += "AID: (não encontrado no PPSE)\n";

    dataPages = 1;
    pageReadSuccess = true;
    pageReadStatus = SUCCESS;
    return true;
}

// Orquestra a leitura ISO-DEP: ATS + NDEF T4T -> DESFire -> EMV.
int ST25R3916::_readIsoDep(rfalNfcDevice *dev) {
    strAllPages = "";
    dataPages = 0;
    totalPages = 0;

    // Extrai ATS (NFC-A) — útil para identificação. RFAL grava o ATS bruto na struct.
    String atsHex;
    if (dev->type == RFAL_NFC_LISTEN_TYPE_NFCA) {
        uint8_t atsLen = dev->proto.isoDep.activation.A.Listener.ATSLen;
        const uint8_t *ats = (const uint8_t *)&dev->proto.isoDep.activation.A.Listener.ATS;
        if (atsLen > 0 && atsLen <= 20) atsHex = _bytesToHex(ats, atsLen);
        ST25R_LOG("ISO-DEP NFC-A ATS(%u)=%s", atsLen, atsHex.c_str());
    } else {
        atsHex = "(NFC-B ISO-DEP)";
    }

    // Tenta NDEF T4T (mais genérico), depois DESFire, depois EMV.
    bool ok = _readNdefT4T();
    if (!ok) ok = _readDESFireInfo();
    if (!ok) ok = _probeEmv();

    if (!ok) {
        // ISO-DEP ativo mas sem aplicação reconhecida — reporta o que se tem.
        printableUID.picc_type = "ISO14443-4";
        strAllPages = "ISO-DEP activated (no known application)\n";
        ST25R_LOG("ISO-DEP: nenhuma aplicação T4T/DESFire/EMV reconhecida");
    }

    // Prefixa a linha de ATS ao dump.
    if (atsHex.length()) strAllPages = "ATS: " + atsHex + "\n" + strAllPages;

    totalPages = dataPages;
    pageReadStatus = SUCCESS;
    pageReadSuccess = true;
    return SUCCESS;
}

static String _bytesToHex(const uint8_t *data, size_t len) {
    String out;
    char buf[4];
    for (size_t i = 0; i < len; i++) {
        sprintf(buf, "%02X", data[i]);
        out += buf;
        if (i < len - 1) out += " ";
    }
    return out;
}

// dump no formato .nfc do Flipper Zero.
int ST25R3916::saveFlipper(String filename) {
    FS *fs;
    if (!getFsStorage(fs)) return FAILURE;

    File file = createNewFile(fs, "/BruceRFID", filename + ".nfc");
    if (!file) return FAILURE;

    String devType = ntagVariant.length() ? ntagVariant : printableUID.picc_type;

    file.println("Filetype: Flipper NFC device");
    file.println("Version: 4");
    file.println(
        "# Device type can be ISO14443-3A, ISO14443-3B, ISO14443-4A, ISO14443-4B, "
        "ISO15693-3, FeliCa, NTAG/Ultralight, Mifare Classic, Mifare DESFire, SLIX, "
        "ST25TB, EMV"
    );
    file.println("Device type: " + devType);
    file.println("# UID is common for all formats");
    file.println("UID: " + printableUID.uid);
    // Flipper grava ATQA em little-endian (ex.: NTAG => "44 00")
    char atqaBuf[6];
    sprintf(atqaBuf, "%02X %02X", uid.atqaByte[0], uid.atqaByte[1]);
    file.println("ATQA: " + String(atqaBuf));
    file.println("SAK: " + printableUID.sak);
    file.println("# Mifare Ultralight specific data");
    file.println("Data format version: 2");
    file.println("Signature: " + _bytesToHex(ntagSignature, 32));
    file.println("Mifare version: " + _bytesToHex(ntagVersion, 8));
    for (int i = 0; i < 3; i++) {
        file.println("Counter " + String(i) + ": " + String(ntagCounters[i]));
        char tBuf[3];
        sprintf(tBuf, "%02X", ntagTearing[i]);
        file.println("Tearing " + String(i) + ": " + String(tBuf));
    }
    file.println("Pages total: " + String(totalPages > 0 ? totalPages : dataPages));
    file.println("Pages read: " + String(dataPages));
    file.print(strAllPages);
    file.println("Failed authentication attempts: 0");

    file.close();
    delay(100);
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
    // NTAG/Ultralight: persiste versão e assinatura p/ emulação fiel (ex.: amiibo).
    if (ntagHasVersion) file.println("Mifare version: " + _bytesToHex(ntagVersion, 8));
    if (ntagHasSignature) file.println("Signature: " + _bytesToHex(ntagSignature, 32));
    if (ntagHasCounters) {
        for (int i = 0; i < 3; i++) file.println("Counter " + String(i) + ": " + String(ntagCounters[i]));
    }
    file.println("# Memory dump");
    file.println("Pages total: " + String(totalPages > 0 ? totalPages : dataPages));
    if (!pageReadSuccess) file.println("Pages read: " + String(dataPages));
    file.print(strAllPages);

    file.close();
    delay(100);
    return SUCCESS;
}

#endif // !LITE_VERSION
