// ============================================================================
// display.cpp —— 显示基础层实现
// ============================================================================

#include "display.h"

#include "board_profile.h"
#include "digit_editor_math.h"

#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#define TFT_RST 13
#define TFT_DC 14
#define TFT_CS 10
#define TFT_SCLK 12
#define TFT_MOSI 11

// ESP32-S3 + TFT_eSPI 2.5.43 direct-register path requires peripheral 2.
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  #if !defined(SPI_PORT) || (SPI_PORT != 2)
    //#error "ESP32-S3 TFT direct-register path requires SPI_PORT=2; define USE_FSPI_PORT"
  #endif
#endif

// ST7735S v1.3 Table 7: 4-line serial minimum write clock cycle 66 ns.
#if defined(SPI_FREQUENCY) && (SPI_FREQUENCY > 15151515UL)
  //#error "ST7735S 4-wire write clock exceeds datasheet 66 ns minimum cycle"
#endif

TFT_eSPI tft = TFT_eSPI();
using namespace ui;

namespace {

int clampInt(int v, int lo, int hi)
{
    if (hi < lo) return lo;
    return v < lo ? lo : (v > hi ? hi : v);
}

// TFT_eSPI::drawString 不会替业务层做换行。这里把任意短 UI 文本先裁成
// 当前像素宽度能容纳的一行，并在被截断时加 "..."。所有基础控件都走这一
// 层，因此即使上游生成了异常长的工程计数法字符串，也不会画出 128px 边界。
void fitText(const char* src, int font, int maxWidth, char* out, size_t outLen)
{
    if (!out || outLen == 0) return;
    out[0] = '\0';
    if (!src || maxWidth <= 0) return;

    if (tft.textWidth(src, font) <= maxWidth) {
        snprintf(out, outLen, "%s", src);
        return;
    }

    static const char* ellipsis = "...";
    const int ellipsisW = tft.textWidth(ellipsis, font);
    if (ellipsisW > maxWidth) return;

    size_t n = 0;
    while (src[n] && n + 4 < outLen) {
        out[n] = src[n];
        out[n + 1] = '\0';
        if (tft.textWidth(out, font) + ellipsisW > maxWidth) {
            out[n] = '\0';
            break;
        }
        ++n;
    }
    strncat(out, ellipsis, outLen - strlen(out) - 1);
}

void drawFitLeft(const char* s, int x, int y, int font, uint16_t fg, uint16_t bg,
                 int maxWidth = -1)
{
    const int W = tft.width();
    const int H = tft.height();
    if (W <= 0 || H <= 0) return;
    x = clampInt(x, 0, W - 1);
    y = clampInt(y, 0, H - 1);
    const int avail = maxWidth < 0 ? (W - x) : (maxWidth < W - x ? maxWidth : W - x);
    char tmp[96];
    fitText(s, font, avail, tmp, sizeof(tmp));
    tft.setTextFont(font);
    tft.setTextColor(fg, bg);
    tft.drawString(tmp, x, y, font);
}

void drawFitRight(const char* s, int xRight, int y, int font, uint16_t fg, uint16_t bg,
                  int maxWidth)
{
    const int W = tft.width();
    const int H = tft.height();
    if (W <= 0 || H <= 0) return;
    xRight = clampInt(xRight, 0, W - 1);
    y = clampInt(y, 0, H - 1);
    const int avail = maxWidth < xRight + 1 ? maxWidth : xRight + 1;
    char tmp[96];
    fitText(s, font, avail, tmp, sizeof(tmp));
    tft.setTextFont(font);
    tft.setTextColor(fg, bg);
    tft.drawRightString(tmp, xRight, y, font);
}

void drawFitCenter(const char* s, int xCenter, int y, int font, uint16_t fg, uint16_t bg,
                   int maxWidth)
{
    const int W = tft.width();
    const int H = tft.height();
    if (W <= 0 || H <= 0) return;
    xCenter = clampInt(xCenter, 0, W - 1);
    y = clampInt(y, 0, H - 1);
    const int avail = maxWidth < W ? maxWidth : W;
    char tmp[96];
    fitText(s, font, avail, tmp, sizeof(tmp));
    tft.setTextFont(font);
    tft.setTextColor(fg, bg);
    tft.drawCentreString(tmp, xCenter, y, font);
}

struct DigitStyle {
    int font;
    int digitW;
    int digitH;
    int gap;
    int totalW;
};

DigitStyle makeDigitStyle(int font, int ndigits)
{
    DigitStyle s{};
    s.font = font;
    s.digitW = tft.textWidth("8", font);
    switch (font) {
    case 7: s.digitH = 48; s.gap = 4; break;
    case 4: s.digitH = 26; s.gap = 4; break;
    case 2: s.digitH = 16; s.gap = 2; break;
    default: s.digitH = 8; s.gap = 2; break;
    }
    s.totalW = ndigits * s.digitW + (ndigits > 0 ? (ndigits - 1) * s.gap : 0);
    return s;
}

// 数位编辑器右侧通常还要给左侧字段名留约 28px。优先保持原来的大字体，
// 只有在 128px portrait 屏放不下时才逐级降到 font 2 / font 1。这样比把一组
// 可逐位编辑的数字硬换行更容易保持位权/光标关系，同时保证左右不越界。
DigitStyle digitStyleFor(int fontH, int ndigits)
{
    const int W = tft.width() > 0 ? tft.width() : (int)kBoard.tftWidth;
    const int maxFieldW = W > 36 ? W - 36 : W;
    const int preferred = fontH >= 40 ? 7 : 4;
    const int candidates[4] = {preferred, preferred == 7 ? 4 : 2, 2, 1};

    DigitStyle last = makeDigitStyle(1, ndigits);
    int previous = -1;
    for (int i = 0; i < 4; ++i) {
        const int font = candidates[i];
        if (font == previous) continue;
        previous = font;
        const DigitStyle s = makeDigitStyle(font, ndigits);
        last = s;
        if (s.totalW <= maxFieldW) return s;
    }
    return last;
}

void trimFixed(char* s)
{
    if (!s) return;
    char* dot = strchr(s, '.');
    if (!dot) return;
    char* end = s + strlen(s);
    while (end > dot + 1 && end[-1] == '0') *--end = '\0';
    if (end > dot && end[-1] == '.') *--end = '\0';
}

}  // namespace

void ui::begin()
{
    Serial.printf("TFT init: TFT_eSPI %s, SPI_PORT=%d, SCLK=%d MOSI=%d CS=%d DC=%d RST=%d @ %lu Hz\n",
                  TFT_ESPI_VERSION, SPI_PORT, TFT_SCLK, TFT_MOSI, TFT_CS, TFT_DC,
                  TFT_RST, (unsigned long)SPI_FREQUENCY);
    tft.init();
    tft.setRotation(kBoard.tftRotation);
    tft.fillScreen(C_BG);
}

void ui::topBar(const char* title, bool bleOn)
{
    const int W = tft.width();
    const int H = 18;
    tft.fillRect(0, 0, W, H, C_PANEL);

    const char* status = bleOn ? "BLE*" : "BLE";
    const int statusW = tft.textWidth(status, 1);
    drawFitLeft(title, 4, 5, 1, C_FG, C_PANEL, W - 12 - statusW);
    drawFitRight(status, W - 4, 5, 1, bleOn ? C_OK : C_DIM, C_PANEL, statusW + 2);
    tft.drawFastHLine(0, H, W, C_AXIS);
}

void ui::bottomHint(const char* hint)
{
    const int W = tft.width();
    const int y = tft.height() - 12;
    tft.fillRect(0, y, W, 12, C_BG);
    drawFitCenter(hint, W / 2, y + 2, 1, C_DIM, C_BG, W - 4);
}

void ui::progressBar(int x, int y, int w, int h, double frac, uint16_t color)
{
    const int W = tft.width();
    const int H = tft.height();
    if (W <= 0 || H <= 0 || w <= 0 || h <= 0) return;
    x = clampInt(x, 0, W - 1);
    y = clampInt(y, 0, H - 1);
    if (x + w > W) w = W - x;
    if (y + h > H) h = H - y;
    if (w <= 0 || h <= 0) return;

    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    const int innerW = w > 4 ? w - 4 : 0;
    const int innerH = h > 4 ? h - 4 : 0;
    const int filled = (int)(innerW * frac + 0.5);
    tft.drawRect(x, y, w, h, C_AXIS);
    if (innerW > 0 && innerH > 0) {
        if (filled > 0)
            tft.fillRect(x + 2, y + 2, filled, innerH, color);
        if (filled < innerW)
            tft.fillRect(x + 2 + filled, y + 2, innerW - filled, innerH, C_BG);
    }
}

void ui::row(int x, int y, int w, const char* label, const char* value, uint16_t color)
{
    const int W = tft.width();
    if (W <= 0 || w <= 0) return;
    x = clampInt(x, 0, W - 1);
    int right = x + w;
    if (right >= W) right = W - 1;
    const int avail = right - x + 1;
    if (avail <= 0) return;

    const int labelW = label ? tft.textWidth(label, 1) : 0;
    const int valueW2 = value ? tft.textWidth(value, 2) : 0;
    if (labelW + 5 + valueW2 <= avail) {
        drawFitLeft(label, x, y + 4, 1, C_DIM, C_BG, avail);
        drawFitRight(value, right, y, 2, color, C_BG, avail - (labelW ? labelW + 5 : 0));
        return;
    }

    // 长数值时改为两行 font 1：第一行字段名，第二行右对齐数值。
    // 128x160 上一行约 8px，两行仍落在普通 18~20px row 高度内。
    drawFitLeft(label, x, y, 1, C_DIM, C_BG, avail);
    drawFitRight(value, right, y + 9, 1, color, C_BG, avail);
}

const char* ui::fmtEng(double v, const char* unit, char* buf, int len, int prec)
{
    if (!buf || len <= 0) return buf;
    if (!unit) unit = "";
    if (!isfinite(v)) {
        snprintf(buf, len, "--%s", unit);
        return buf;
    }
    if (v == 0.0) {
        snprintf(buf, len, "0%s", unit);
        return buf;
    }

    // 2022 SI prefixes, restricted to engineering exponents (multiples of 3).
    // ASCII 'u' is intentional: the ST7735 built-in ASCII font does not provide
    // a reliable micro sign on every TFT_eSPI setup.
    static const char* prefix[] = {
        "q", "r", "y", "z", "a", "f", "p", "n", "u", "m", "",
        "k", "M", "G", "T", "P", "E", "Z", "Y", "R", "Q"
    };
    static constexpr int minExp = -30;
    static constexpr int maxExp = 30;

    const double av = fabs(v);
    int exp3 = (int)floor(log10(av) / 3.0) * 3;
    if (exp3 < minExp) exp3 = minExp;
    if (exp3 > maxExp) exp3 = maxExp;
    double scaled = v / pow(10.0, exp3);

    int digits = prec < 1 ? 1 : (prec > 9 ? 9 : prec);
    int order = scaled == 0.0 ? 0 : (int)floor(log10(fabs(scaled)));
    int decimals = digits - order - 1;
    if (decimals < 0) decimals = 0;
    if (decimals > 9) decimals = 9;

    char number[40];
    snprintf(number, sizeof(number), "%.*f", decimals, scaled);
    trimFixed(number);

    // 舍入后 999.x 可能变成 1000；提升一个工程前缀，避免出现 "1000pF"。
    if (fabs(strtod(number, nullptr)) >= 1000.0 && exp3 < maxExp) {
        exp3 += 3;
        scaled = v / pow(10.0, exp3);
        order = scaled == 0.0 ? 0 : (int)floor(log10(fabs(scaled)));
        decimals = digits - order - 1;
        if (decimals < 0) decimals = 0;
        if (decimals > 9) decimals = 9;
        snprintf(number, sizeof(number), "%.*f", decimals, scaled);
        trimFixed(number);
    }

    const int idx = (exp3 - minExp) / 3;
    snprintf(buf, len, "%s%s%s", number, prefix[idx], unit);
    return buf;
}

const char* ui::fmtFreq(double hz, char* buf, int len)
{
    return fmtEng(hz, "Hz", buf, len, 4);
}

const char* ui::fmtDeg(double deg, char* buf, int len)
{
    snprintf(buf, len, "%+.1fdeg", deg);
    return buf;
}

void DigitEditor::setup(int32_t vmin, int32_t vmax, int ndigits, int32_t v)
{
    m_vmin = vmin;
    m_vmax = vmax;
    m_ndigits = ndigits > 7 ? 7 : (ndigits < 1 ? 1 : ndigits);
    m_value = clampValue(v);
    m_pos = m_ndigits - 1;
    syncFromValue();
}

void DigitEditor::syncFromValue()
{
    int32_t v = m_value;
    for (int i = m_ndigits - 1; i >= 0; --i) {
        m_digits[i] = (uint8_t)(v % 10);
        v /= 10;
    }
}

bool DigitEditor::onEvent(InputEvent e)
{
    switch (e) {
    case InputEvent::Up:
        if (m_pos > 0) { --m_pos; return true; }
        return false;
    case InputEvent::Down:
        if (m_pos < m_ndigits - 1) { ++m_pos; return true; }
        return false;
    case InputEvent::EncInc:
    case InputEvent::EncDec: {
        const int32_t next = digitEditorStep(
            m_value, m_vmin, m_vmax, m_ndigits, m_pos,
            e == InputEvent::EncInc ? 1 : -1);
        if (next == m_value) return false;
        m_value = next;
        syncFromValue();
        return true;
    }
    default:
        return false;
    }
}

int DigitEditor::width(int fontH) const
{
    return digitStyleFor(fontH, m_ndigits).totalW;
}

void DigitEditor::draw(int x, int y, int fontH, bool focused) const
{
    const DigitStyle style = digitStyleFor(fontH, m_ndigits);
    const int W = tft.width();
    const int H = tft.height();

    // 保留 2px underline 边距，并把整组数字作为一个块钳在可视区域内。
    const int maxX = W - style.totalW - 2;
    const int drawX = clampInt(x, 2, maxX < 2 ? 2 : maxX);
    const int maxY = H - style.digitH - 7;
    const int drawY = clampInt(y, 0, maxY < 0 ? 0 : maxY);

    for (int i = 0; i < m_ndigits; ++i) {
        const int dx = drawX + i * (style.digitW + style.gap);
        bool leading = true;
        for (int j = 0; j < i; ++j) if (m_digits[j] != 0) { leading = false; break; }
        if (m_digits[i] != 0 || i == m_ndigits - 1) leading = false;
        const uint16_t col = focused ? C_FG : (leading ? C_GRID : C_DIM);

        tft.setTextFont(style.font);
        tft.setTextColor(col, C_BG);
        char s[2] = {(char)('0' + m_digits[i]), 0};
        tft.drawString(s, dx, drawY, style.font);

        const uint16_t underline = !focused ? C_BG : (i == m_pos ? C_ACCENT : C_GRID);
        const int uy = drawY + style.digitH + 2;
        if (uy < H)
            tft.fillRect(dx - 2, uy, style.digitW + 4, 4, underline);
    }
}

void Checkbox::setup(const char* labelOff, const char* labelOn, bool v)
{
    m_labelOff = labelOff;
    m_labelOn = labelOn;
    m_value = v;
}

bool Checkbox::onEvent(InputEvent e)
{
    if (e == InputEvent::EncInc || e == InputEvent::EncDec) {
        toggle();
        return true;
    }
    return false;
}

void Checkbox::draw(int x, int y, bool focused) const
{
    const int W = tft.width();
    const int H = tft.height();
    const int box = 16;
    x = clampInt(x, 2, W - box - 2);
    y = clampInt(y, 2, H - box - 2);
    tft.drawRect(x - 2, y - 2, box + 4, box + 4, focused ? C_ACCENT : C_BG);
    tft.drawRect(x, y, box, box, C_FG);
    if (m_value) tft.fillRect(x + 3, y + 3, box - 6, box - 6, C_OK);
    else tft.fillRect(x + 3, y + 3, box - 6, box - 6, C_BG);
    drawFitLeft(m_value ? m_labelOn : m_labelOff, x + box + 8, y, 2,
                focused ? C_FG : C_DIM, C_BG, W - (x + box + 10));
}
