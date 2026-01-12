#include "tft.h"

#if defined(USE_ARDUINO_GFX)

tft_display::tft_display(int16_t _W, int16_t _H) : _height(_H), _width(_W) {
    bus = new Arduino_HWSPI(14, 5, 13, 15, -1, &SPI);
    _gfx = new Arduino_ST7789(bus, TFT_RST, ROTATION, 0, _width, _height, 52, 40, 53, 40);
}

void tft_display::begin(uint32_t speed) {
    (void)speed;
    if (_gfx) {
        _gfx->begin();
        _width = _gfx->width();
        _height = _gfx->height();
    }
}

void tft_display::init(uint8_t tc) {
    (void)tc;
    begin();
}

void tft_display::setRotation(uint8_t r) {
    if (_gfx) _gfx->setRotation(r);
    _rotation = r;
}

void tft_display::drawPixel(int32_t x, int32_t y, uint32_t color) {
    if (_gfx) _gfx->drawPixel(x, y, color);
}

void tft_display::drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
    if (_gfx) _gfx->drawLine(x0, y0, x1, y1, color);
}

void tft_display::drawFastHLine(int32_t x, int32_t y, int32_t w, uint32_t color) {
    if (_gfx) _gfx->drawFastHLine(x, y, w, color);
}

void tft_display::drawFastVLine(int32_t x, int32_t y, int32_t h, uint32_t color) {
    if (_gfx) _gfx->drawFastVLine(x, y, h, color);
}

void tft_display::drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    if (_gfx) _gfx->drawRect(x, y, w, h, color);
}

void tft_display::fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    if (_gfx) _gfx->fillRect(x, y, w, h, color);
}

void tft_display::fillRectHGradient(
    int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color1, uint32_t color2
) {
    (void)color2;
    fillRect(x, y, w, h, color1);
}

void tft_display::fillRectVGradient(
    int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color1, uint32_t color2
) {
    (void)color2;
    fillRect(x, y, w, h, color1);
}

void tft_display::fillScreen(uint32_t color) {
    if (_gfx) _gfx->fillScreen(color);
}

void tft_display::drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color) {
    if (_gfx) _gfx->drawRoundRect(x, y, w, h, r, color);
}

void tft_display::fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color) {
    if (_gfx) _gfx->fillRoundRect(x, y, w, h, r, color);
}

void tft_display::drawCircle(int32_t x0, int32_t y0, int32_t r, uint32_t color) {
    if (_gfx) _gfx->drawCircle(x0, y0, r, color);
}

void tft_display::fillCircle(int32_t x0, int32_t y0, int32_t r, uint32_t color) {
    if (_gfx) _gfx->fillCircle(x0, y0, r, color);
}

void tft_display::drawTriangle(
    int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color
) {
    if (_gfx) _gfx->drawTriangle(x0, y0, x1, y1, x2, y2, color);
}

void tft_display::fillTriangle(
    int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color
) {
    if (_gfx) _gfx->fillTriangle(x0, y0, x1, y1, x2, y2, color);
}

void tft_display::drawEllipse(int16_t x0, int16_t y0, int32_t rx, int32_t ry, uint16_t color) {
    drawEllipseFallback(x0, y0, rx, ry, color);
}

void tft_display::fillEllipse(int16_t x0, int16_t y0, int32_t rx, int32_t ry, uint16_t color) {
    fillEllipseFallback(x0, y0, rx, ry, color);
}

void tft_display::drawArc(
    int32_t x, int32_t y, int32_t r, int32_t ir, uint32_t startAngle, uint32_t endAngle, uint32_t fg_color,
    uint32_t bg_color, bool smoothArc
) {
    (void)bg_color;
    (void)smoothArc;
    _gfx->fillArc(x, y, r, ir, startAngle + 90, endAngle + 90, fg_color);
}

void tft_display::drawWideLine(
    float ax, float ay, float bx, float by, float wd, uint32_t fg_color, uint32_t bg_color
) {
    (void)bg_color;
    drawWideLineFallback(ax, ay, bx, by, wd, fg_color);
}

void tft_display::drawXBitmap(
    int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color
) {
    if (!_gfx || !bitmap) return;
    _gfx->drawXBitmap(x, y, bitmap, w, h, color);
}

void tft_display::drawXBitmap(
    int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color, uint16_t bg
) {
    if (!_gfx || !bitmap) return;
    for (int16_t j = 0; j < h; ++j) {
        for (int16_t i = 0; i < w; ++i) {
            uint8_t byte = bitmap[(j * ((w + 7) / 8)) + (i >> 3)];
            bool bit = byte & (0x80 >> (i & 7));
            _gfx->drawPixel(x + i, y + j, bit ? color : bg);
        }
    }
}

void tft_display::pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *data) {
    pushImageFallback(x, y, w, h, data);
}

void tft_display::pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *data) {
    pushImageFallback(x, y, w, h, data);
}

void tft_display::pushImage(
    int32_t x, int32_t y, int32_t w, int32_t h, uint8_t *data, bool bpp8, uint16_t *cmap
) {
    if (!_gfx || !data || !bpp8 || !cmap) return;
    for (int32_t row = 0; row < h; ++row) {
        for (int32_t col = 0; col < w; ++col) {
            uint8_t idx = data[row * w + col];
            _gfx->drawPixel(x + col, y + row, cmap[idx]);
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
    if (_gfx) _gfx->invertDisplay(i);
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

void tft_display::setCursor(int16_t x, int16_t y) {
    if (_gfx) _gfx->setCursor(x, y);
}

int16_t tft_display::getCursorX() const { return _gfx ? _gfx->getCursorX() : 0; }

int16_t tft_display::getCursorY() const { return _gfx ? _gfx->getCursorY() : 0; }

void tft_display::setTextSize(uint8_t s) {
    _textSize = s ? s : 1;
    if (_gfx) _gfx->setTextSize(_textSize);
}

void tft_display::setTextColor(uint16_t c) {
    _textColor = c;
    if (_gfx) _gfx->setTextColor(c);
}

void tft_display::setTextColor(uint16_t c, uint16_t b, bool bgfill) {
    (void)bgfill;
    _textColor = c;
    _textBgColor = b;
    if (_gfx) _gfx->setTextColor(c, b);
}

void tft_display::setTextDatum(uint8_t d) { _textDatum = d; }

uint8_t tft_display::getTextDatum() const { return _textDatum; }

void tft_display::setTextFont(uint8_t f) { _textFont = f; }

void tft_display::setTextWrap(bool wrapX, bool wrapY) {
    (void)wrapY;
    if (_gfx) _gfx->setTextWrap(wrapX);
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

size_t tft_display::write(uint8_t c) { return _gfx ? _gfx->write(c) : 0; }

size_t tft_display::write(const uint8_t *buffer, size_t size) {
    if (!_gfx || !buffer) return 0;
    size_t written = 0;
    for (size_t i = 0; i < size; ++i) { written += _gfx->write(buffer[i]); }
    return written;
}

size_t tft_display::println() { return _gfx ? _gfx->println() : 0; }

size_t tft_display::printf(const char *fmt, ...) {
    if (!_gfx || !fmt) return 0;
    va_list args;
    va_start(args, fmt);
    char stackBuf[128];
    int len = vsnprintf(stackBuf, sizeof(stackBuf), fmt, args);
    va_end(args);
    if (len < 0) return 0;
    if (static_cast<size_t>(len) < sizeof(stackBuf)) { return _gfx->print(stackBuf); }
    std::unique_ptr<char[]> buf(new char[len + 1]);
    va_start(args, fmt);
    vsnprintf(buf.get(), len + 1, fmt, args);
    va_end(args);
    return _gfx->print(buf.get());
}

int16_t tft_display::width() const { return _gfx ? _gfx->width() : 0; }

int16_t tft_display::height() const { return _gfx ? _gfx->height() : 0; }

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

Arduino_GFX *tft_display::native() { return nullptr; }

void tft_display::drawWideLineFallback(float ax, float ay, float bx, float by, float wd, uint32_t color) {
    if (!_gfx) return;
    float dx = bx - ax;
    float dy = by - ay;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len == 0) {
        _gfx->drawPixel(static_cast<int32_t>(ax), static_cast<int32_t>(ay), color);
        return;
    }
    float nx = -dy / len;
    float ny = dx / len;
    int32_t half = static_cast<int32_t>(std::ceil(wd / 2.0f));
    for (int32_t i = -half; i <= half; ++i) {
        float ox = nx * static_cast<float>(i);
        float oy = ny * static_cast<float>(i);
        _gfx->drawLine(
            static_cast<int32_t>(ax + ox),
            static_cast<int32_t>(ay + oy),
            static_cast<int32_t>(bx + ox),
            static_cast<int32_t>(by + oy),
            color
        );
    }
}

void tft_display::drawEllipseFallback(int16_t x0, int16_t y0, int32_t rx, int32_t ry, uint32_t color) {
    if (!_gfx || rx < 0 || ry < 0) return;
    int32_t rx2 = rx * rx;
    int32_t ry2 = ry * ry;
    int32_t x = 0;
    int32_t y = ry;
    int64_t px = 0;
    int64_t py = 2LL * rx2 * y;
    plot4EllipsePoints(x0, y0, x, y, color);
    int64_t p = std::llround(ry2 - (rx2 * ry) + (0.25 * rx2));
    while (px < py) {
        ++x;
        px += 2LL * ry2;
        if (p < 0) p += ry2 + px;
        else {
            --y;
            py -= 2LL * rx2;
            p += ry2 + px - py;
        }
        plot4EllipsePoints(x0, y0, x, y, color);
    }
    p = std::llround(ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) - rx2 * ry2);
    while (y > 0) {
        --y;
        py -= 2LL * rx2;
        if (p > 0) p += rx2 - py;
        else {
            ++x;
            px += 2LL * ry2;
            p += rx2 - py + px;
        }
        plot4EllipsePoints(x0, y0, x, y, color);
    }
}

void tft_display::fillEllipseFallback(int16_t x0, int16_t y0, int32_t rx, int32_t ry, uint32_t color) {
    if (!_gfx || rx < 0 || ry < 0) return;
    int32_t rx2 = rx * rx;
    int32_t ry2 = ry * ry;
    int32_t x = 0;
    int32_t y = ry;
    int64_t px = 0;
    int64_t py = 2LL * rx2 * y;
    int64_t p = std::llround(ry2 - (rx2 * ry) + (0.25 * rx2));
    while (px < py) {
        ++x;
        px += 2LL * ry2;
        if (p < 0) p += ry2 + px;
        else {
            --y;
            py -= 2LL * rx2;
            p += ry2 + px - py;
        }
        _gfx->drawFastHLine(x0 - x, y0 + y, x * 2, color);
        _gfx->drawFastHLine(x0 - x, y0 - y, x * 2, color);
    }
    p = std::llround(ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) - rx2 * ry2);
    while (y >= 0) {
        _gfx->drawFastHLine(x0 - x, y0 + y, x * 2, color);
        _gfx->drawFastHLine(x0 - x, y0 - y, x * 2, color);
        --y;
        py -= 2LL * rx2;
        if (p > 0) p += rx2 - py;
        else {
            ++x;
            px += 2LL * ry2;
            p += rx2 - py + px;
        }
    }
}

void tft_display::plot4EllipsePoints(int16_t cx, int16_t cy, int32_t x, int32_t y, uint32_t color) {
    _gfx->drawPixel(cx + x, cy + y, color);
    _gfx->drawPixel(cx - x, cy + y, color);
    _gfx->drawPixel(cx + x, cy - y, color);
    _gfx->drawPixel(cx - x, cy - y, color);
}

int16_t tft_display::drawAlignedString(const String &s, int32_t x, int32_t y, uint8_t datum) {
    if (!_gfx) return 0;
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
    _gfx->setCursor(cx, cy);
    _gfx->print(s);
    return w;
}

tft_sprite::tft_sprite(tft_display *parent) : _display(parent) {}

void *tft_sprite::createSprite(int16_t w, int16_t h, uint8_t frames) {
    (void)frames;
    if (w <= 0 || h <= 0) return nullptr;
    _width = w;
    _height = h;
    _buffer.assign(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
    return _buffer.data();
}

void tft_sprite::deleteSprite() {
    _buffer.clear();
    _buffer.shrink_to_fit();
    _width = _height = 0;
}

void tft_sprite::fillScreen(uint32_t color) { fillRect(0, 0, _width, _height, color); }

void tft_sprite::setColorDepth(uint8_t depth) { _colorDepth = depth; }

void tft_sprite::setCursor(int16_t x, int16_t y) {
    _cursorX = x;
    _cursorY = y;
}

void tft_sprite::setTextColor(uint16_t c) { _textColor = c; }

void tft_sprite::setTextColor(uint16_t c, uint16_t b) {
    _textColor = c;
    _textBgColor = b;
}

void tft_sprite::setTextSize(uint8_t s) { _textSize = s ? s : 1; }

void tft_sprite::setTextDatum(uint8_t d) { _textDatum = d; }

void tft_sprite::fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    if (!_hasBuffer()) return;
    if (w <= 0 || h <= 0) return;
    int32_t x0 = std::max<int32_t>(0, x);
    int32_t y0 = std::max<int32_t>(0, y);
    int32_t x1 = std::min<int32_t>(_width, x + w);
    int32_t y1 = std::min<int32_t>(_height, y + h);
    uint16_t c = static_cast<uint16_t>(color);
    for (int32_t row = y0; row < y1; ++row) {
        uint16_t *line = &_buffer[static_cast<size_t>(row) * static_cast<size_t>(_width)];
        std::fill(line + x0, line + x1, c);
    }
}

void tft_sprite::drawFastVLine(int32_t x, int32_t y, int32_t h, uint32_t color) {
    fillRect(x, y, 1, h, color);
}

void tft_sprite::fillCircle(int32_t x0, int32_t y0, int32_t r, uint32_t color) {
    if (!_hasBuffer() || r < 0) return;
    uint16_t c = static_cast<uint16_t>(color);
    for (int32_t y = -r; y <= r; ++y) {
        int32_t dx = static_cast<int32_t>(std::sqrt(static_cast<float>(r * r - y * y)));
        drawHLine(x0 - dx, y0 + y, dx * 2, c);
    }
}

void tft_sprite::drawCircle(int32_t x0, int32_t y0, int32_t r, uint32_t color) {
    if (!_hasBuffer() || r < 0) return;
    int32_t f = 1 - r;
    int32_t ddF_x = 1;
    int32_t ddF_y = -2 * r;
    int32_t x = 0;
    int32_t y = r;
    auto plot = [&](int32_t px, int32_t py) { setPixel(px, py, static_cast<uint16_t>(color)); };
    plot(x0, y0 + r);
    plot(x0, y0 - r);
    plot(x0 + r, y0);
    plot(x0 - r, y0);
    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        plot(x0 + x, y0 + y);
        plot(x0 - x, y0 + y);
        plot(x0 + x, y0 - y);
        plot(x0 - x, y0 - y);
        plot(x0 + y, y0 + x);
        plot(x0 - y, y0 + x);
        plot(x0 + y, y0 - x);
        plot(x0 - y, y0 - x);
    }
}

void tft_sprite::fillEllipse(int16_t x0, int16_t y0, int32_t rx, int32_t ry, uint16_t color) {
    if (!_hasBuffer() || rx < 0 || ry < 0) return;
    int32_t rx2 = rx * rx;
    int32_t ry2 = ry * ry;
    int32_t x = 0;
    int32_t y = ry;
    int64_t px = 0;
    int64_t py = 2LL * rx2 * y;
    int64_t p = std::llround(ry2 - (rx2 * ry) + (0.25 * rx2));
    while (px < py) {
        ++x;
        px += 2LL * ry2;
        if (p < 0) p += ry2 + px;
        else {
            --y;
            py -= 2LL * rx2;
            p += ry2 + px - py;
        }
        drawHLine(x0 - x, y0 + y, x * 2, color);
        drawHLine(x0 - x, y0 - y, x * 2, color);
    }
    p = std::llround(ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) - rx2 * ry2);
    while (y >= 0) {
        drawHLine(x0 - x, y0 + y, x * 2, color);
        drawHLine(x0 - x, y0 - y, x * 2, color);
        --y;
        py -= 2LL * rx2;
        if (p > 0) p += rx2 - py;
        else {
            ++x;
            px += 2LL * ry2;
            p += rx2 - py + px;
        }
    }
}

void tft_sprite::fillTriangle(
    int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color
) {
    if (!_hasBuffer()) return;
    uint16_t c = static_cast<uint16_t>(color);
    if (y0 > y1) {
        std::swap(y0, y1);
        std::swap(x0, x1);
    }
    if (y1 > y2) {
        std::swap(y1, y2);
        std::swap(x1, x2);
    }
    if (y0 > y1) {
        std::swap(y0, y1);
        std::swap(x0, x1);
    }

    auto drawSpan = [&](int32_t y, int32_t xa, int32_t xb) {
        if (y < 0 || y >= _height) return;
        if (xa > xb) std::swap(xa, xb);
        xa = std::max<int32_t>(0, xa);
        xb = std::min<int32_t>(_width - 1, xb);
        drawHLine(xa, y, xb - xa + 1, c);
    };

    auto edge = [](int32_t y0, int32_t x0, int32_t y1, int32_t x1, int32_t y) -> int32_t {
        if (y1 == y0) return x0;
        return x0 + (x1 - x0) * (y - y0) / (y1 - y0);
    };

    for (int32_t y = y0; y <= y1; ++y) {
        int32_t xa = edge(y0, x0, y2, x2, y);
        int32_t xb = edge(y0, x0, y1, x1, y);
        drawSpan(y, xa, xb);
    }
    for (int32_t y = y1; y <= y2; ++y) {
        int32_t xa = edge(y0, x0, y2, x2, y);
        int32_t xb = edge(y1, x1, y2, x2, y);
        drawSpan(y, xa, xb);
    }
}

void tft_sprite::pushSprite(int32_t x, int32_t y, uint32_t transparent) {
    if (!_hasBuffer() || !_display) return;
    if (transparent == TFT_TRANSPARENT) {
        for (int32_t j = 0; j < _height; ++j) {
            const uint16_t *row = &_buffer[static_cast<size_t>(j) * static_cast<size_t>(_width)];
            for (int32_t i = 0; i < _width; ++i) {
                uint16_t color = row[i];
                if (color != static_cast<uint16_t>(transparent)) { _display->drawPixel(x + i, y + j, color); }
            }
        }
    } else {
        _display->pushImage(x, y, _width, _height, _buffer.data());
    }
}

void tft_sprite::pushToSprite(tft_sprite *dest, int32_t x, int32_t y, uint32_t transparent) {
    if (!dest || !_hasBuffer() || !dest->_hasBuffer()) return;
    for (int32_t j = 0; j < _height; ++j) {
        for (int32_t i = 0; i < _width; ++i) {
            uint16_t color =
                _buffer[static_cast<size_t>(j) * static_cast<size_t>(_width) + static_cast<size_t>(i)];
            if (color == static_cast<uint16_t>(transparent)) continue;
            dest->setPixel(x + i, y + j, color);
        }
    }
}

int16_t tft_sprite::width() const { return _width; }

int16_t tft_sprite::height() const { return _height; }

int16_t tft_sprite::fontHeight(int16_t font) const {
    (void)font;
    return static_cast<int16_t>(_textSize * 8);
}

void tft_sprite::drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
    if (!_hasBuffer()) return;
    int32_t dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int32_t dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int32_t err = dx + dy;
    while (true) {
        setPixel(x0, y0, static_cast<uint16_t>(color));
        if (x0 == x1 && y0 == y1) break;
        int32_t e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void tft_sprite::drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    drawLine(x, y, x + w - 1, y, color);
    drawLine(x, y, x, y + h - 1, color);
    drawLine(x + w - 1, y, x + w - 1, y + h - 1, color);
    drawLine(x, y + h - 1, x + w - 1, y + h - 1, color);
}

void tft_sprite::drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color) {
    (void)r;
    drawRect(x, y, w, h, color);
}

void tft_sprite::fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color) {
    (void)r;
    fillRect(x, y, w, h, color);
}

void tft_sprite::drawPixel(int32_t x, int32_t y, uint32_t color) {
    setPixel(x, y, static_cast<uint16_t>(color));
}

void tft_sprite::drawXBitmap(
    int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color, uint16_t bg
) {
    if (!_hasBuffer() || !bitmap) return;
    for (int16_t j = 0; j < h; ++j) {
        for (int16_t i = 0; i < w; ++i) {
            uint8_t byte = bitmap[(j * ((w + 7) / 8)) + (i >> 3)];
            bool bit = byte & (0x80 >> (i & 7));
            setPixel(x + i, y + j, bit ? color : bg);
        }
    }
}

void tft_sprite::pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *data) {
    if (!_hasBuffer() || !data) return;
    for (int32_t row = 0; row < h; ++row) {
        for (int32_t col = 0; col < w; ++col) { setPixel(x + col, y + row, data[row * w + col]); }
    }
}

void tft_sprite::pushImage(
    int32_t x, int32_t y, int32_t w, int32_t h, uint8_t *data, bool bpp8, uint16_t *cmap
) {
    if (!_hasBuffer() || !data || !bpp8 || !cmap) return;
    for (int32_t row = 0; row < h; ++row) {
        for (int32_t col = 0; col < w; ++col) {
            uint8_t idx = data[row * w + col];
            setPixel(x + col, y + row, cmap[idx]);
        }
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
    fillRect(x, y, w, h, color1);
}

void tft_sprite::fillRectVGradient(
    int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color1, uint32_t color2
) {
    (void)color2;
    fillRect(x, y, w, h, color1);
}

int16_t tft_sprite::drawString(const String &string, int32_t x, int32_t y, uint8_t font) {
    (void)font;
    setCursor(x, y);
    return string.length();
}

bool tft_sprite::_hasBuffer() const { return !_buffer.empty() && _width > 0 && _height > 0; }

void tft_sprite::setPixel(int32_t x, int32_t y, uint16_t color) {
    if (x < 0 || y < 0 || x >= _width || y >= _height) return;
    _buffer[static_cast<size_t>(y) * static_cast<size_t>(_width) + static_cast<size_t>(x)] = color;
}

void tft_sprite::drawHLine(int32_t x, int32_t y, int32_t w, uint16_t color) {
    if (y < 0 || y >= _height || w <= 0) return;
    int32_t x0 = std::max<int32_t>(0, x);
    int32_t x1 = std::min<int32_t>(_width, x + w);
    if (x0 >= x1) return;
    std::fill(
        _buffer.begin() + static_cast<size_t>(y) * static_cast<size_t>(_width) + x0,
        _buffer.begin() + static_cast<size_t>(y) * static_cast<size_t>(_width) + x1,
        color
    );
}

#endif
