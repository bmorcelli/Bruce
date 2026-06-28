#pragma once
#if !defined(LITE_VERSION)

#include "RFIDInterface.h"
#include <SPI.h>
#include <Wire.h>
#include <vector>
#include <rfal_nfc.h>
#include <rfal_rfst25r3916.h>

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

    void stopDiscovery();

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
};

#endif // !LITE_VERSION
