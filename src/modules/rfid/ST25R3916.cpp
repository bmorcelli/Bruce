#include "ST25R3916.h"
#if !defined(LITE_VERSION)

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
}

static void _setNfcPower(bool enabled) {
#if defined(IO_EXP_NFC) && IO_EXP_NFC >= 0
    ioExpander.setPinDirection(IO_EXP_NFC, OUTPUT);
    ioExpander.turnPinOnOff(IO_EXP_NFC, enabled ? HIGH : LOW);
#endif
}

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
    delete _hw;
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

    if (state != RFAL_NFC_STATE_ACTIVATED) {
        return TAG_NOT_PRESENT;
    }

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

int ST25R3916::clone() { return NOT_IMPLEMENTED; }
int ST25R3916::erase() { return NOT_IMPLEMENTED; }
int ST25R3916::write(int) { return NOT_IMPLEMENTED; }
int ST25R3916::write_ndef() { return NOT_IMPLEMENTED; }
int ST25R3916::emulate() { return NOT_IMPLEMENTED; }
int ST25R3916::load() { return NOT_IMPLEMENTED; }
int ST25R3916::save(String) { return NOT_IMPLEMENTED; }

#endif // !LITE_VERSION
