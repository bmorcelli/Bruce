#pragma once
#if !defined(LITE_VERSION)

#include "RFIDInterface.h"
#include <SPI.h>
#include <Wire.h>
#include <rfal_nfc.h>
#include <rfal_rfst25r3916.h>
#include <vector>

class ST25R3916 : public RFIDInterface {
public:
    enum CONNECTION_TYPE { SPI_MODE = 0, I2C_MODE = 1 };

    static constexpr uint8_t I2C_ADDRESS = 0x50;

    ST25R3916(CONNECTION_TYPE connection_type = SPI_MODE);
    ~ST25R3916();

    bool begin() override;
    int read(int cardBaudRate = 0) override;
    int clone() override;
    int erase() override;
    int write(int cardBaudRate = 0) override;
    int write_ndef() override;
    int emulate() override;
    int load() override;
    int save(String filename) override;
    int saveFlipper(String filename) override;

    void stopDiscovery();

    // Milestone 1 — advanced NFC-A info (NTAG / Ultralight)
    String ntagVariant;        // "NTAG213", "NTAG215", "NTAG216", "MF Ultralight", ...
    uint8_t ntagVersion[8];    // raw GET_VERSION response
    uint8_t ntagSignature[32]; // raw READ_SIG response (ECC-P256)
    uint32_t ntagCounters[3];  // monotonic counters 0/1/2
    uint8_t ntagTearing[3];    // tearing flags per counter
    bool ntagHasVersion = false;
    bool ntagHasSignature = false;
    bool ntagHasCounters = false;
    int _ntagPagesHint = 0; // page count derived from GET_VERSION (0 = unknown)

private:
    CONNECTION_TYPE _connection_type;
    RfalRfST25R3916Class *_hw = nullptr;
    RfalNfcClass *_nfc = nullptr;
    SPIClass *_spi = nullptr;
    bool _discoveryStarted = false;

    bool _initSPI();
    bool _initI2C();
    bool _startDiscovery();
    bool _pollForTag(rfalNfcDevice **dev, uint32_t timeoutMs = 5000);
    void _logOpControl(const char *where);
    void _probeField(const char *where);
    void _parseDevice(rfalNfcDevice *dev);
    String _getNfcaTypeName(uint8_t sak);
    void _parseLoadedData();
    int _readDataBlocks(rfalNfcDevice *dev);
    int _writeUltralight(rfalNfcDevice *dev);
    int _eraseUltralight(rfalNfcDevice *dev);
    int _writeNdefBlocks(rfalNfcDevice *dev);
    bool _buildLoadedNdefMessage(std::vector<uint8_t> &ndefOut);
    bool _writeT2TPage(uint8_t page, const uint8_t data[4], bool verify = true);
    bool _isUltralightUserPage(int page) const;
    bool _writeMagicGen1UID(rfalNfcDevice *dev);
    bool _writeMagicGen2UID(rfalNfcDevice *dev);

    // helpers
    String _getNtagVariant();
    bool _readNtagSignature();
    bool _readNtagCounters();

    // ISO15693 (NFC-V), NFC-B e NFC-F (FeliCa)
    int _readNfcV(rfalNfcDevice *dev);
    void _parseNfcB(rfalNfcDevice *dev);
    int _readFeliCa(rfalNfcDevice *dev);

    // ISO-DEP / Type 4 Tag (T4T): DESFire, NDEF T4T, EMV
    int _readIsoDep(rfalNfcDevice *dev);
    bool _isoDepApdu(const uint8_t *tx, uint16_t txLen, uint8_t *rx, uint16_t rxCap, uint16_t *rxLen);
    bool _readNdefT4T();
    bool _readDESFireInfo();
    bool _probeEmv();

    // Emulation NFC-A
    bool _setupListenMode(const uint8_t *uidBuf, uint8_t uidLen, const uint8_t *atqa, uint8_t sak);
    void _listenStop();
    int _handleListenLoop(uint32_t timeoutMs);
    bool _listenRespond(const uint8_t *resp, uint16_t len);
    int _buildEmuPages();

    uint8_t _emuPages[256][4];
    int _emuPageCount = 0;
};

#endif // !LITE_VERSION
