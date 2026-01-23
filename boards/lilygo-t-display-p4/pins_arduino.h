#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include "soc/soc_caps.h"
#include <stdint.h>

// BOOT_MODE 35
// BOOT_MODE2 36 pullup

#define SDM SD_MMC

static const uint8_t TX = 37;
static const uint8_t RX = 38;

// Use GPIOs 36 or lower on the P4 DevKit to avoid LDO power issues with high numbered GPIOs.
static const uint8_t SS = 26;
static const uint8_t MOSI = 32;
static const uint8_t MISO = 33;
static const uint8_t SCK = 36;

static const uint8_t A0 = 16;
static const uint8_t A1 = 17;
static const uint8_t A2 = 18;
static const uint8_t A3 = 19;
static const uint8_t A4 = 20;
static const uint8_t A5 = 21;
static const uint8_t A6 = 22;
static const uint8_t A7 = 23;
static const uint8_t A8 = 49;
static const uint8_t A9 = 50;
static const uint8_t A10 = 51;
static const uint8_t A11 = 52;
static const uint8_t A12 = 53;
static const uint8_t A13 = 54;

static const uint8_t T0 = 2;
static const uint8_t T1 = 3;
static const uint8_t T2 = 4;
static const uint8_t T3 = 5;
static const uint8_t T4 = 6;
static const uint8_t T5 = 7;
static const uint8_t T6 = 8;
static const uint8_t T7 = 9;
static const uint8_t T8 = 10;
static const uint8_t T9 = 11;
static const uint8_t T10 = 12;
static const uint8_t T11 = 13;
static const uint8_t T12 = 14;
static const uint8_t T13 = 15;

/* ESP32-P4 EV Function board specific definitions */
// ETH
#define ETH_PHY_TYPE ETH_PHY_TLK110
#define ETH_PHY_ADDR 1
#define ETH_PHY_MDC 31
#define ETH_PHY_MDIO 52
#define ETH_PHY_POWER 51
#define ETH_RMII_TX_EN 49
#define ETH_RMII_TX0 34
#define ETH_RMII_TX1 35
#define ETH_RMII_RX0 29
#define ETH_RMII_RX1_EN 30
#define ETH_RMII_CRS_DV 28
#define ETH_RMII_CLK 50
#define ETH_CLK_MODE EMAC_CLK_EXT_IN

// WIFI - ESP32C6
#define BOARD_HAS_SDIO_ESP_HOSTED
#define BOARD_SDIO_ESP_HOSTED_CLK 12
#define BOARD_SDIO_ESP_HOSTED_CMD 13
#define BOARD_SDIO_ESP_HOSTED_D0 11
#define BOARD_SDIO_ESP_HOSTED_D1 10
#define BOARD_SDIO_ESP_HOSTED_D2 9
#define BOARD_SDIO_ESP_HOSTED_D3 8
#define BOARD_SDIO_ESP_HOSTED_RESET 15

// IIC
#define IIC_1_SDA 7
#define IIC_1_SCL 8
#define IIC_2_SDA 20
#define IIC_2_SCL 21

// BOOT
#define ESP32P4_BOOT 35

// XL9535
#define XL9535_SDA IIC_1_SDA
#define XL9535_SCL IIC_1_SCL
#define XL9535_INT 5
// XL9535引脚功能
// #define XL9535_3_3_V_POWER_EN Cpp_Bus_Driver::Xl95x5::Pin::IO0
// #define XL9535_SKY13453_VCTL Cpp_Bus_Driver::Xl95x5::Pin::IO1
// #define XL9535_SCREEN_RST Cpp_Bus_Driver::Xl95x5::Pin::IO2
// #define XL9535_TOUCH_RST Cpp_Bus_Driver::Xl95x5::Pin::IO3
// #define XL9535_TOUCH_INT Cpp_Bus_Driver::Xl95x5::Pin::IO4
// #define XL9535_ETHERNET_RST Cpp_Bus_Driver::Xl95x5::Pin::IO5
// #define XL9535_5_0_V_POWER_EN Cpp_Bus_Driver::Xl95x5::Pin::IO6
// #define XL9535_EXTERNAL_SENSOR_INT Cpp_Bus_Driver::Xl95x5::Pin::IO7
// #define XL9535_ESP32P4_VCCA_POWER_EN Cpp_Bus_Driver::Xl95x5::Pin::IO10
// #define XL9535_GPS_WAKE_UP Cpp_Bus_Driver::Xl95x5::Pin::IO11
// #define XL9535_RTC_INT Cpp_Bus_Driver::Xl95x5::Pin::IO12
// #define XL9535_ESP32C6_WAKE_UP Cpp_Bus_Driver::Xl95x5::Pin::IO13
// #define XL9535_ESP32C6_EN Cpp_Bus_Driver::Xl95x5::Pin::IO14
// #define XL9535_SD_EN Cpp_Bus_Driver::Xl95x5::Pin::IO15
// #define XL9535_SX1262_RST Cpp_Bus_Driver::Xl95x5::Pin::IO16
// #define XL9535_SX1262_DIO1 Cpp_Bus_Driver::Xl95x5::Pin::IO17

// ES8311
#define ES8311_SDA IIC_2_SDA
#define ES8311_SCL IIC_2_SCL
#define ES8311_ADC_DATA 11
#define ES8311_DAC_DATA 10
#define ES8311_BCLK 12
#define ES8311_MCLK 13
#define ES8311_WS_LRCK 9

// AW86224
#define AW86224_SDA IIC_2_SDA
#define AW86224_SCL IIC_2_SCL

// SGM38121
#define SGM38121_SDA IIC_2_SDA
#define SGM38121_SCL IIC_2_SCL

// PCF8563
#define PCF8563_SDA IIC_1_SDA
#define PCF8563_SCL IIC_1_SCL

// BQ27220
#define BQ27220_SDA IIC_1_SDA
#define BQ27220_SCL IIC_1_SCL

// SPI
#define SPI_1_SCLK 2
#define SPI_1_MOSI 3
#define SPI_1_MISO 4

// SX1262
#define SX1262_CS 24
#define SX1262_BUSY 6
#define SX1262_SCLK SPI_1_SCLK
#define SX1262_MOSI SPI_1_MOSI
#define SX1262_MISO SPI_1_MISO

// L76K
#define GPS_TX 22
#define GPS_RX 23

// ICM20948
#define ICM20948_SDA IIC_2_SDA
#define ICM20948_SCL IIC_2_SCL

// HI8561
#define HI8561_SCREEN_BL 51
#define HI8561_TOUCH_SDA IIC_1_SDA
#define HI8561_TOUCH_SCL IIC_1_SCL

// GT9895
#define GT9895_TOUCH_SDA IIC_1_SDA
#define GT9895_TOUCH_SCL IIC_1_SCL

// Camera
#define CAMERA_SDA IIC_2_SDA
#define CAMERA_SCL IIC_2_SCL

// SDIO
#define SDIO_1_CLK 43
#define SDIO_1_CMD 44
#define SDIO_1_D0 39
#define SDIO_1_D1 40
#define SDIO_1_D2 41
#define SDIO_1_D3 42

#define SDIO_2_CLK 18
#define SDIO_2_CMD 19
#define SDIO_2_D0 14
#define SDIO_2_D1 15
#define SDIO_2_D2 16
#define SDIO_2_D3 17

// SD
// SDMMC
#define SD_SDIO_CLK SDIO_1_CLK
#define SD_SDIO_CMD SDIO_1_CMD
#define SD_SDIO_D0 SDIO_1_D0
#define SD_SDIO_D1 SDIO_1_D1
#define SD_SDIO_D2 SDIO_1_D2
#define SD_SDIO_D3 SDIO_1_D3
// SDSPI
#define SD_SCLK SDIO_1_CLK
#define SD_MOSI SDIO_1_CMD
#define SD_MISO SDIO_1_D0
#define SD_CS SDIO_1_D3

// ESP32C6 SDIO
#define ESP32C6_SDIO_CLK SDIO_2_CLK
#define ESP32C6_SDIO_CMD SDIO_2_CMD
#define ESP32C6_SDIO_D0 SDIO_2_D0
#define ESP32C6_SDIO_D1 SDIO_2_D1
#define ESP32C6_SDIO_D2 SDIO_2_D2
#define ESP32C6_SDIO_D3 SDIO_2_D3

// Extended io
#define EXT_2X8P_SPI_SCLK SPI_1_SCLK
#define EXT_2X8P_SPI_MOSI SPI_1_MOSI
#define EXT_2X8P_SPI_MISO SPI_1_MISO

#define EXT_2X8P_IO_26 26
#define EXT_2X8P_IO_27 27
#define EXT_2X8P_IO_33 33
#define EXT_2X8P_IO_32 32
#define EXT_2X8P_IO_25 25
#define EXT_2X8P_IO_36 36
#define EXT_2X8P_IO_53 53
#define EXT_2X8P_IO_54 54
#define EXT_1X4P_1_IO_47 47
#define EXT_1X4P_1_IO_48 48
#define EXT_1X4P_2_IO_45 45
#define EXT_1X4P_2_IO_46 46

////////////////////////////////////////////////// gpio config
/////////////////////////////////////////////////////

////////////////////////////////////////////////// other define config
/////////////////////////////////////////////////////

// XL9535
#define XL9535_IIC_ADDRESS 0x20

// ES8311
#define ES8311_IIC_ADDRESS 0x18

// AW86224
#define AW86224_IIC_ADDRESS 0x58

// SGM38121
#define SGM38121_IIC_ADDRESS 0x28

// PCF8563
#define PCF8563_IIC_ADDRESS 0x51

// BQ27220
#define BQ27220_IIC_ADDRESS 0x55

// ICM20948
#define ICM20948_IIC_ADDRESS 0x68

// HI8561
#define HI8561_SCREEN_WIDTH 540
#define HI8561_SCREEN_HEIGHT 1168
#define HI8561_SCREEN_MIPI_DSI_DPI_CLK_MHZ 60
#define HI8561_SCREEN_MIPI_DSI_HSYNC 28
#define HI8561_SCREEN_MIPI_DSI_HBP 26
#define HI8561_SCREEN_MIPI_DSI_HFP 20
#define HI8561_SCREEN_MIPI_DSI_VSYNC 2
#define HI8561_SCREEN_MIPI_DSI_VBP 22
#define HI8561_SCREEN_MIPI_DSI_VFP 200
#define HI8561_SCREEN_DATA_LANE_NUM 2
#define HI8561_SCREEN_LANE_BIT_RATE_MBPS 1000
#define HI8561_TOUCH_IIC_ADDRESS 0x68

// RM69A10
#define RM69A10_SCREEN_WIDTH 568
#define RM69A10_SCREEN_HEIGHT 1232
#define RM69A10_SCREEN_MIPI_DSI_DPI_CLK_MHZ 60
#define RM69A10_SCREEN_MIPI_DSI_HSYNC 50
#define RM69A10_SCREEN_MIPI_DSI_HBP 150
#define RM69A10_SCREEN_MIPI_DSI_HFP 50
#define RM69A10_SCREEN_MIPI_DSI_VSYNC 40
#define RM69A10_SCREEN_MIPI_DSI_VBP 120
#define RM69A10_SCREEN_MIPI_DSI_VFP 80
#define RM69A10_SCREEN_DATA_LANE_NUM 2
#define RM69A10_SCREEN_LANE_BIT_RATE_MBPS 1000

// GT9895
#define GT9895_IIC_ADDRESS 0x5D
#define GT9895_MAX_X_SIZE 1060
#define GT9895_MAX_Y_SIZE 2400
#define GT9895_X_SCALE_FACTOR static_cast<float>(RM69A10_SCREEN_WIDTH) / static_cast<float>(GT9895_MAX_X_SIZE)
#define GT9895_Y_SCALE_FACTOR                                                                                \
    static_cast<float>(RM69A10_SCREEN_HEIGHT) / static_cast<float>(GT9895_MAX_Y_SIZE)

// CAMERA
#define CAMERA_WIDTH 1920
#define CAMERA_HEIGHT 1080
// #define CAMERA_WIDTH 1280
// #define CAMERA_HEIGHT 720
// #define CAMERA_WIDTH 800
// #define CAMERA_HEIGHT 800
// #define CAMERA_WIDTH 640
// #define CAMERA_HEIGHT 480

#define CAMERA_DATA_LANE_NUM 2
#define CAMERA_LANE_BIT_RATE_MBPS 1000
#define CAMERA_MIPI_DSI_DPI_CLK_MHZ 60

/* BRUCE PINS */

#ifndef DEVICE_NAME
#define DEVICE_NAME "Lilygo T-Display P4"
#endif

// Definitions for TFT
#ifndef T_DISPLAY_AMOLED
#define USE_ARDUINO_GFX 1
#define TFT_DATABUS_N 4
#define TFT_HSYNC_PULSE_WIDTH 28
#define TFT_HSYNC_BACK_PORCH 26
#define TFT_HSYNC_FRONT_PORCH 20
#define TFT_VSYNC_PULSE_WIDTH 2
#define TFT_VSYNC_BACK_PORCH 22
#define TFT_VSYNC_FRONT_PORCH 200
#define TFT_PREF_SPEED 60000000

#define TFT_DISPLAY_DRIVER_N 50
#define TFT_WIDTH 540
#define TFT_HEIGHT 1168
#define TFT_RST -1
#define TFT_DSI_INIT hi8561_lcd_init

#else
// Definitions for AMOLED
#define USE_ARDUINO_GFX 1
#define TFT_DATABUS_N 4
#define TFT_HSYNC_PULSE_WIDTH 50
#define TFT_HSYNC_BACK_PORCH 150
#define TFT_HSYNC_FRONT_PORCH 50
#define TFT_VSYNC_PULSE_WIDTH 40
#define TFT_VSYNC_BACK_PORCH 120
#define TFT_VSYNC_FRONT_PORCH 80
#define TFT_PREF_SPEED 60000000

#define TFT_DISPLAY_DRIVER_N 50
#define TFT_WIDTH 568
#define TFT_HEIGHT 1232
#define TFT_RST -1
#define TFT_DSI_INIT rm69a10_lcd_init_cmd

#endif
#define SPI_SS_PIN 25
#define SPI_MOSI_PIN 3
#define SPI_MISO_PIN 4
#define SPI_SCK_PIN 2

#define SDCARD_CS -1
#define SDCARD_SCK -1
#define SDCARD_MISO -1
#define SDCARD_MOSI -1

#define CC1101_GDO0_PIN 26
#define CC1101_SS_PIN 27
#define CC1101_MOSI_PIN SPI_MOSI_PIN
#define CC1101_SCK_PIN SPI_SCK_PIN
#define CC1101_MISO_PIN SPI_MISO_PIN

#define NRF24_CE_PIN 33
#define NRF24_SS_PIN 32
#define NRF24_MOSI_PIN SPI_MOSI_PIN
#define NRF24_SCK_PIN SPI_SCK_PIN
#define NRF24_MISO_PIN SPI_MISO_PIN

#define USE_W5500_VIA_SPI // which ethernet module it uses?
#define W5500_INT_PIN 25  //??
#define W5500_SS_PIN 36   //??
#define W5500_MOSI_PIN SPI_MOSI_PIN
#define W5500_SCK_PIN SPI_SCK_PIN
#define W5500_MISO_PIN SPI_MISO_PIN

// Set Main I2C Bus
#define GROVE_SDA 7
#define GROVE_SCL 8
static const uint8_t SDA = GROVE_SDA;
static const uint8_t SCL = GROVE_SCL;

// Serial
#define SERIAL_TX 21
#define SERIAL_RX 16

// Infrared
#define TXLED -1
#define RXLED -1

#endif /* Pins_Arduino_h */
