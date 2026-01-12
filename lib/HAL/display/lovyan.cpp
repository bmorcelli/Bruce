#include "tft.h"

#if defined(USE_LOVYANGFX)

tft_display::tft_display(int16_t _W, int16_t _H) : _height(_H), _width(_W) {}

void tft_display::begin(uint32_t speed) {
    (void)speed;

    {
        auto cfg = _bus_instance.config();

        cfg.spi_host = HSPI_HOST;
        cfg.spi_mode = 0;
        cfg.freq_write = 40000000;
        cfg.freq_read = 15000000;
        cfg.spi_3wire = true;
        cfg.use_lock = true;
        cfg.dma_channel = SPI_DMA_CH_AUTO;
        cfg.pin_sclk = TFT_SCLK;
        cfg.pin_mosi = TFT_MOSI;
        cfg.pin_miso = -1;
        cfg.pin_dc = TFT_DC;

        _bus_instance.config(cfg);
        _panel_instance.setBus(&_bus_instance);
    }

    {
        auto cfg = _panel_instance.config();
        cfg.pin_cs = TFT_CS;
        cfg.pin_rst = TFT_RST;
        cfg.pin_busy = -1;
        cfg.memory_width = 240;
        cfg.memory_height = 320;
        cfg.panel_width = TFT_WIDTH;
        cfg.panel_height = TFT_HEIGHT;
        cfg.offset_x = 52;
        cfg.offset_y = 40;
        cfg.offset_rotation = 0;
        cfg.readable = true;
        cfg.invert = false;
        cfg.rgb_order = false;
        cfg.dlen_16bit = false;
        cfg.bus_shared = true;
        _panel_instance.config(cfg);
        setPanel(&_panel_instance);
    }
    lgfx::LGFX_Device::begin();
}

void tft_display::init(uint8_t tc) {
    (void)tc;
    begin();
}

void tft_display::setRotation(uint8_t r) {
    lgfx::LGFX_Device::setRotation(r);
    _rotation = r;
}

void tft_display::drawPixel(int32_t x, int32_t y, uint32_t color) {
    lgfx::LGFX_Device::drawPixel(x, y, (uint16_t)color);
}

void tft_display::drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
    lgfx::LGFX_Device::drawLine(x0, y0, x1, y1, (uint16_t)color);
}

void tft_display::drawFastHLine(int32_t x, int32_t y, int32_t w, uint32_t color) {
    lgfx::LGFX_Device::drawFastHLine(x, y, w, (uint16_t)color);
}

void tft_display::drawFastVLine(int32_t x, int32_t y, int32_t h, uint32_t color) {
    lgfx::LGFX_Device::drawFastVLine(x, y, h, (uint16_t)color);
}

void tft_display::drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    lgfx::LGFX_Device::drawRect(x, y, w, h, (uint16_t)color);
}

void tft_display::fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    lgfx::LGFX_Device::fillRect(x, y, w, h, (uint16_t)color);
}

void tft_display::fillRectHGradient(
    int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color1, uint32_t color2
) {
    (void)color2;
    fillRect(x, y, w, h, (uint16_t)color1);
}

void tft_display::fillRectVGradient(
    int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color1, uint32_t color2
) {
    (void)color2;
    fillRect(x, y, w, h, (uint16_t)color1);
}

void tft_display::fillScreen(uint32_t color) { lgfx::LGFX_Device::fillScreen((uint16_t)color); }

void tft_display::drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color) {
    lgfx::LGFX_Device::drawRoundRect(x, y, w, h, r, (uint16_t)color);
}

void tft_display::fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color) {
    lgfx::LGFX_Device::fillRoundRect(x, y, w, h, r, (uint16_t)color);
}

void tft_display::drawCircle(int32_t x0, int32_t y0, int32_t r, uint32_t color) {
    lgfx::LGFX_Device::drawCircle(x0, y0, r, (uint16_t)color);
}

void tft_display::fillCircle(int32_t x0, int32_t y0, int32_t r, uint32_t color) {
    lgfx::LGFX_Device::fillCircle(x0, y0, r, (uint16_t)color);
}

void tft_display::drawTriangle(
    int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color
) {
    lgfx::LGFX_Device::drawTriangle(x0, y0, x1, y1, x2, y2, (uint16_t)color);
}

void tft_display::fillTriangle(
    int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color
) {
    lgfx::LGFX_Device::fillTriangle(x0, y0, x1, y1, x2, y2, (uint16_t)color);
}

void tft_display::drawEllipse(int16_t x0, int16_t y0, int32_t rx, int32_t ry, uint16_t color) {
    lgfx::LGFX_Device::drawEllipse(x0, y0, rx, ry, (uint16_t)color);
}

void tft_display::fillEllipse(int16_t x0, int16_t y0, int32_t rx, int32_t ry, uint16_t color) {
    lgfx::LGFX_Device::fillEllipse(x0, y0, rx, ry, (uint16_t)color);
}

void tft_display::drawArc(
    int32_t x, int32_t y, int32_t r, int32_t ir, uint32_t startAngle, uint32_t endAngle, uint32_t fg_color,
    uint32_t bg_color, bool smoothArc
) {
    (void)bg_color;
    (void)smoothArc;
    lgfx::LGFX_Device::fillArc(x, y, r, ir, startAngle + 90, endAngle + 90, (uint16_t)fg_color);
}

void tft_display::drawWideLine(
    float ax, float ay, float bx, float by, float wd, uint32_t fg_color, uint32_t bg_color
) {
    (void)bg_color;
    lgfx::LGFX_Device::drawWideLine(ax, ay, bx, by, wd, (uint16_t)fg_color);
}

void tft_display::drawXBitmap(
    int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color
) {
    if (!bitmap) return;
    lgfx::LGFX_Device::drawXBitmap(x, y, bitmap, w, h, (uint16_t)color);
}

void tft_display::drawXBitmap(
    int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color, uint16_t bg
) {
    if (!bitmap) return;
    lgfx::LGFX_Device::drawXBitmap(x, y, bitmap, w, h, (uint16_t)color, (uint16_t)bg);
}

void tft_display::pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *data) {
    lgfx::LGFX_Device::pushImage(x, y, w, h, data);
}

void tft_display::pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *data) {
    lgfx::LGFX_Device::pushImage(x, y, w, h, data);
}

void tft_display::pushImage(
    int32_t x, int32_t y, int32_t w, int32_t h, uint8_t *data, bool bpp8, uint16_t *cmap
) {
    if (!data || !bpp8 || !cmap) return;
    for (int32_t row = 0; row < h; ++row) {
        for (int32_t col = 0; col < w; ++col) {
            uint8_t idx = data[row * w + col];
            uint16_t color = cmap[idx];
            if (_swapBytes) color = static_cast<uint16_t>((color >> 8) | (color << 8));
            lgfx::LGFX_Device::drawPixel(x + col, y + row, (uint16_t)color);
        }
    }
}

void tft_display::pushImage(
    int32_t x, int32_t y, int32_t w, int32_t h, const uint8_t *data, bool bpp8, uint16_t *cmap
) {
    pushImage(x, y, w, h, const_cast<uint8_t *>(data), bpp8, cmap);
}

void tft_display::invertDisplay(bool i) {
    (void)i;
    lgfx::LGFX_Device::invertDisplay(i);
}

void tft_display::setSwapBytes(bool swap) { _swapBytes = swap; }

bool tft_display::getSwapBytes() const { return _swapBytes; }

uint16_t tft_display::color565(uint8_t r, uint8_t g, uint8_t b) const {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

int16_t tft_display::textWidth(const String &s, uint8_t font) const {
    (void)font;
    return static_cast<int16_t>(s.length() * 6 * _textSize);
}

int16_t tft_display::textWidth(const char *s, uint8_t font) const {
    (void)font;
    return static_cast<int16_t>(strlen(s) * 6 * _textSize);
}

void tft_display::setCursor(int16_t x, int16_t y) { lgfx::LGFX_Device::setCursor(x, y); }

int16_t tft_display::getCursorX() const { return lgfx::LGFX_Device::getCursorX(); }

int16_t tft_display::getCursorY() const { return lgfx::LGFX_Device::getCursorY(); }

void tft_display::setTextSize(uint8_t s) {
    _textSize = s ? s : 1;
    lgfx::LGFX_Device::setTextSize(_textSize);
}

void tft_display::setTextColor(uint16_t c) {
    _textColor = c;
    lgfx::LGFX_Device::setTextColor((uint16_t)c);
}

void tft_display::setTextColor(uint16_t c, uint16_t b, bool bgfill) {
    (void)bgfill;
    _textColor = c;
    _textBgColor = b;
    lgfx::LGFX_Device::setTextColor((uint16_t)c, (uint16_t)b);
}

void tft_display::setTextDatum(uint8_t d) { _textDatum = d; }

uint8_t tft_display::getTextDatum() const { return _textDatum; }

void tft_display::setTextFont(uint8_t f) { _textFont = f; }

void tft_display::setTextWrap(bool wrapX, bool wrapY) {
    (void)wrapY;
    lgfx::LGFX_Device::setTextWrap(wrapX);
}

int16_t tft_display::drawString(const String &string, int32_t x, int32_t y, uint8_t font) {
    (void)font;
    return drawAlignedString(string, x, y, _textDatum);
}

int16_t tft_display::drawCentreString(const String &string, int32_t x, int32_t y, uint8_t font) {
    (void)font;
    return drawAlignedString(string, x, y, TC_DATUM);
}

int16_t tft_display::drawRightString(const String &string, int32_t x, int32_t y, uint8_t font) {
    (void)font;
    return drawAlignedString(string, x, y, TR_DATUM);
}

size_t tft_display::write(uint8_t c) { return lgfx::LGFX_Device::write(c); }

size_t tft_display::write(const uint8_t *buffer, size_t size) {
    return lgfx::LGFX_Device::write(buffer, size);
}

size_t tft_display::println() { return lgfx::LGFX_Device::println(); }

size_t tft_display::printf(const char *fmt, ...) {
    if (!fmt) return 0;
    va_list args;
    va_start(args, fmt);
    char stackBuf[128];
    int len = vsnprintf(stackBuf, sizeof(stackBuf), fmt, args);
    va_end(args);
    if (len < 0) return 0;
    if (static_cast<size_t>(len) < sizeof(stackBuf)) { return lgfx::LGFX_Device::print(stackBuf); }
    std::unique_ptr<char[]> buf(new char[len + 1]);
    va_start(args, fmt);
    vsnprintf(buf.get(), len + 1, fmt, args);
    va_end(args);
    return lgfx::LGFX_Device::print(buf.get());
}

int16_t tft_display::width() const { return lgfx::LGFX_Device::width(); }

int16_t tft_display::height() const { return lgfx::LGFX_Device::height(); }

SPIClass &tft_display::getSPIinstance() const { return SPI; }

void tft_display::writecommand(uint8_t c) { (void)c; }

uint32_t tft_display::getTextColor() const { return _textColor; }

uint32_t tft_display::getTextBgColor() const { return _textBgColor; }

uint8_t tft_display::getTextSize() const { return _textSize; }

uint8_t tft_display::getRotation() const { return _rotation; }

int16_t tft_display::fontHeight(int16_t font) const {
    (void)font;
    return static_cast<int16_t>(_textSize * 8);
}

lgfx::LGFX_Device *tft_display::native() { return nullptr; }

int16_t tft_display::drawAlignedString(const String &s, int32_t x, int32_t y, uint8_t datum) {
    int16_t w = static_cast<int16_t>(s.length() * 6 * _textSize);
    int16_t h = static_cast<int16_t>(8 * _textSize);
    int32_t cx = x;
    int32_t cy = y;
    switch (datum) {
        case TC_DATUM: cx -= w / 2; break;
        case TR_DATUM: cx -= w; break;
        case MC_DATUM:
            cx -= w / 2;
            cy -= h / 2;
            break;
        case MR_DATUM:
            cx -= w;
            cy -= h / 2;
            break;
        case BC_DATUM:
            cx -= w / 2;
            cy -= h;
            break;
        case BR_DATUM:
            cx -= w;
            cy -= h;
            break;
        case BL_DATUM: cy -= h; break;
        default: break;
    }
    lgfx::LGFX_Device::setCursor(cx, cy);
    lgfx::LGFX_Device::print(s);
    return w;
}

tft_sprite::tft_sprite(tft_display *parent)
    : lgfx::LGFX_Sprite(parent ? static_cast<lgfx::LGFX_Device *>(parent) : nullptr) {}

void *tft_sprite::createSprite(int16_t w, int16_t h, uint8_t frames) {
    (void)frames;
    return lgfx::LGFX_Sprite::createSprite(w, h);
}

void tft_sprite::deleteSprite() { lgfx::LGFX_Sprite::deleteSprite(); }

void tft_sprite::setColorDepth(uint8_t depth) { lgfx::LGFX_Sprite::setColorDepth(depth); }

void tft_sprite::fillScreen(uint32_t color) { lgfx::LGFX_Sprite::fillScreen((uint16_t)color); }

void tft_sprite::fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    lgfx::LGFX_Sprite::fillRect(x, y, w, h, (uint16_t)color);
}

void tft_sprite::fillCircle(int32_t x, int32_t y, int32_t r, uint32_t color) {
    lgfx::LGFX_Sprite::fillCircle(x, y, r, (uint16_t)color);
}

void tft_sprite::fillEllipse(int16_t x, int16_t y, int32_t rx, int32_t ry, uint16_t color) {
    lgfx::LGFX_Sprite::fillEllipse(x, y, rx, ry, (uint16_t)color);
}

void tft_sprite::fillTriangle(
    int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color
) {
    lgfx::LGFX_Sprite::fillTriangle(x0, y0, x1, y1, x2, y2, (uint16_t)color);
}

void tft_sprite::drawFastVLine(int32_t x, int32_t y, int32_t h, uint32_t color) {
    lgfx::LGFX_Sprite::drawFastVLine(x, y, h, (uint16_t)color);
}

void tft_sprite::pushSprite(int32_t x, int32_t y, uint32_t transparent) {
    lgfx::LGFX_Sprite::pushSprite(x, y, (uint16_t)transparent);
}

void tft_sprite::pushToSprite(tft_sprite *dest, int32_t x, int32_t y, uint32_t transparent) {
    lgfx::LGFX_Sprite::pushSprite(static_cast<lgfx::LGFX_Sprite *>(dest), x, y, (uint16_t)transparent);
}

void tft_sprite::pushImage(
    int32_t x, int32_t y, int32_t w, int32_t h, uint8_t *data, bool bpp8, uint16_t *cmap
) {
    if (!data || !bpp8 || !cmap) return;
    for (int32_t row = 0; row < h; ++row) {
        for (int32_t col = 0; col < w; ++col) { drawPixel(x + col, y + row, cmap[data[row * w + col]]); }
    }
}

void tft_sprite::pushImage(
    int32_t x, int32_t y, int32_t w, int32_t h, const uint8_t *data, bool bpp8, uint16_t *cmap
) {
    pushImage(x, y, w, h, const_cast<uint8_t *>(data), bpp8, cmap);
}

void tft_sprite::fillRectHGradient(
    int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color1, uint32_t color2
) {
    (void)color2;
    lgfx::LGFX_Sprite::fillRect(x, y, w, h, (uint16_t)color1);
}

void tft_sprite::fillRectVGradient(
    int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color1, uint32_t color2
) {
    lgfx::LGFX_Sprite::fillRect(x, y, w, h, (uint16_t)color1);
}

int16_t tft_sprite::width() const { return static_cast<int16_t>(lgfx::LGFX_Sprite::width()); }

int16_t tft_sprite::height() const { return static_cast<int16_t>(lgfx::LGFX_Sprite::height()); }

#endif
