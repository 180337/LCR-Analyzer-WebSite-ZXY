// ============================================================================
// screen_twoport.cpp —— 模式 3：双端口扫频（H=Vout/Vin）-> seal -> BLE
// ----------------------------------------------------------------------------
// canonical 数据是复数、无量纲的 H。TFT/网站显示均从复 H 推导：
//   gain[dB] = 20*log10(|H|), phase[deg] = atan2(Im H, Re H)。
// W 链为 raw/no-calib，CSV 标注 raw_w_path。主 loop 是 BLE poll 唯一 owner。
// TFT 动态区采用 dirty 检测并限制为最多 5 Hz；sweep.poll() 仍每圈执行。
// ============================================================================

#include "screens.h"
#include "radio_manager.h"

#include <Arduino.h>
#include <math.h>
#include <stdio.h>

TwoPortScreen screenTwoPort;

namespace {
constexpr int kCfgX = 5;
constexpr int kCfgY0 = 31;
constexpr int kCfgDY = 36;
constexpr double kMinHMag = 1e-12;   // preview 仅用于 log10 数值保护（-240 dB）
constexpr uint32_t kUiRefreshMinMs = 200;
}

void TwoPortScreen::onEnter()
{
    static bool inited = false;
    if (!inited) {
        m_f0.setup((int32_t)INSTRUMENT_F_MIN_HZ, 9999, 5, 100);
        m_f1.setup(11, (int32_t)INSTRUMENT_F_MAX_HZ, 5, 2000);
        m_ppd.setup(1, 50, 2, 10);
        inited = true;
    }
    m_field = 0;
    m_phase = Phase::Config;
    drawConfig();
}

void TwoPortScreen::drawConfigField(int i)
{
    if (i < 0 || i > 2) return;
    DigitEditor* eds[3] = {&m_f0, &m_f1, &m_ppd};
    const char* labels[3] = {"F0", "F1", "PPD"};
    const bool focused = (m_field == i);
    const int y = kCfgY0 + i * kCfgDY;
    tft.setTextFont(1);
    tft.setTextColor(focused ? ui::C_ACCENT : ui::C_DIM, ui::C_BG);
    tft.drawString(labels[i], kCfgX, y + 8);
    int ex = tft.width() - eds[i]->width(26) - 5;
    if (ex < 28) ex = 28;
    eds[i]->draw(ex, y, 26, focused);
}

void TwoPortScreen::drawConfig()
{
    tft.fillScreen(ui::C_BG);
    ui::topBar("TWO-PORT H", radio.state() != RadioState::Off);
    for (int i = 0; i < 3; ++i) drawConfigField(i);

    if (millis() < m_errUntilMs) {
        tft.setTextFont(1);
        tft.setTextColor(ui::C_ERR, ui::C_BG);
        tft.drawCentreString("NEED F0<F1 10Hz..10k", tft.width() / 2, 132, 1);
    }
    ui::bottomHint("ENC:EDIT UD:FLD OK:RUN");
}

bool TwoPortScreen::startSweep()
{
    const int32_t f0 = m_f0.value(), f1 = m_f1.value();
    if (f0 >= f1 || f0 < (int32_t)INSTRUMENT_F_MIN_HZ ||
        f1 > (int32_t)INSTRUMENT_F_MAX_HZ) {
        m_errUntilMs = millis() + 2000;
        drawConfig();
        return false;
    }
    if (radio.state() != RadioState::Off) {
        radio.stopBle();
        if (radio.state() != RadioState::Off) return false;
    }

    SweepConfig cfg{};
    cfg.kind = MeasurementKind::TwoPortTransfer;
    cfg.fStartHz = (double)f0;
    cfg.fStopHz = (double)f1;
    cfg.pointsPerDecade = (uint16_t)m_ppd.value();
    cfg.maxPoints = SWEEP_MAX_POINTS;
    if (sweep.start(cfg) != SweepStatus::Ok) {
        m_errUntilMs = millis() + 2000;
        drawConfig();
        return false;
    }
    m_phase = Phase::Run;
    drawRun();
    return true;
}

void TwoPortScreen::drawRun()
{
    tft.fillScreen(ui::C_BG);
    ui::topBar("H SWEEP", false);
    tft.setTextFont(1);
    tft.setTextColor(ui::C_DIM, ui::C_BG);
    tft.drawString("RADIO OFF", 7, 24);
    tft.drawString("f:", 7, 48);
    tft.drawString("pts:", 7, 66);
    tft.drawString("err:", 7, 84);
    ui::progressBar(7, 108, tft.width() - 14, 12, 0.0, ui::C_ACCENT);
    ui::bottomHint("BACK:STOP AFTER BLOCK");

    m_runUiValid = false;
    m_lastRunUiMs = 0;
    updateRun(false, true);
}

void TwoPortScreen::updateRun(bool stopping, bool force)
{
    const uint32_t now = millis();
    const double freq = sweep.currentFreqHz();
    const uint16_t done = sweep.completedPoints();
    const uint16_t total = sweep.totalPoints();
    const uint16_t err = sweep.errorCount();

    const bool freqChanged = !m_runUiValid || freq != m_runUiFreq;
    const bool doneChanged = !m_runUiValid || done != m_runUiDone || total != m_runUiTotal;
    const bool errChanged = !m_runUiValid || err != m_runUiErr;
    const bool stopChanged = !m_runUiValid || stopping != m_runUiStopping;
    if (!freqChanged && !doneChanged && !errChanged && !stopChanged) return;

    const bool urgent = !m_runUiValid || stopChanged;
    if (!force && !urgent && (uint32_t)(now - m_lastRunUiMs) < kUiRefreshMinMs)
        return;

    char buf[28];
    tft.setTextFont(1);
    if (freqChanged) {
        tft.fillRect(30, 46, tft.width() - 34, 14, ui::C_BG);
        tft.setTextColor(ui::C_FG, ui::C_BG);
        snprintf(buf, sizeof(buf), "%.6g Hz", freq);
        tft.drawString(buf, 30, 48);
    }
    if (doneChanged) {
        tft.fillRect(30, 64, tft.width() - 34, 14, ui::C_BG);
        tft.setTextColor(ui::C_FG, ui::C_BG);
        snprintf(buf, sizeof(buf), "%d/%d", (int)done, (int)total);
        tft.drawString(buf, 30, 66);
        ui::progressBar(7, 108, tft.width() - 14, 12,
                        total ? (double)done / total : 0.0, ui::C_ACCENT);
    }
    if (errChanged) {
        tft.fillRect(30, 82, tft.width() - 34, 14, ui::C_BG);
        tft.setTextColor(ui::C_FG, ui::C_BG);
        snprintf(buf, sizeof(buf), "%d", (int)err);
        tft.drawString(buf, 30, 84);
    }
    if (stopChanged) {
        tft.fillRect(7, 24, tft.width() - 14, 12, ui::C_BG);
        tft.setTextColor(stopping ? ui::C_CH2 : ui::C_DIM, ui::C_BG);
        tft.drawString(stopping ? "STOPPING..." : "RADIO OFF", 7, 24);
    }

    m_runUiFreq = freq;
    m_runUiDone = done;
    m_runUiTotal = total;
    m_runUiErr = err;
    m_runUiStopping = stopping;
    m_runUiValid = true;
    m_lastRunUiMs = now;
}

void TwoPortScreen::drawPreview()
{
    const TwoPortDataset* d = sweep.sealedTwoPortDataset();
    if (!d || d->nPoints < 2) return;

    static double f[SWEEP_MAX_POINTS];
    static double gainDb[SWEEP_MAX_POINTS];
    static double phaseDeg[SWEEP_MAX_POINTS];
    double gLo = 1e300, gHi = -1e300;
    double pLo = 1e300, pHi = -1e300;

    const uint16_t n = d->nPoints < SWEEP_MAX_POINTS ? d->nPoints : SWEEP_MAX_POINTS;
    double prevPhase = 0.0;
    for (uint16_t i = 0; i < n; ++i) {
        f[i] = d->points[i].f;
        double mag = hypot(d->points[i].reH, d->points[i].imH);
        if (!isfinite(mag) || mag < kMinHMag) mag = kMinHMag;
        gainDb[i] = 20.0 * log10(mag);

        double ph = atan2(d->points[i].imH, d->points[i].reH) * 180.0 / M_PI;
        if (i > 0) {
            // Bode 连线采用最邻近 unwrap，避免 +180/-180 处贯穿全图的假跳线。
            while (ph - prevPhase > 180.0) ph -= 360.0;
            while (ph - prevPhase < -180.0) ph += 360.0;
        }
        phaseDeg[i] = ph;
        prevPhase = ph;

        if (gainDb[i] < gLo) gLo = gainDb[i];
        if (gainDb[i] > gHi) gHi = gainDb[i];
        if (ph < pLo) pLo = ph;
        if (ph > pHi) pHi = ph;
    }
    if (!(gHi > gLo)) { gLo -= 1.0; gHi += 1.0; }
    else { const double pad = 0.08 * (gHi - gLo); gLo -= pad; gHi += pad; }
    if (!(pHi > pLo)) { pLo -= 10.0; pHi += 10.0; }
    else { const double pad = 0.08 * (pHi - pLo); pLo -= pad; pHi += pad; }

    // Portrait 128x160：左轴 18px，右轴保留约 25px 给 phase 数字。
    m_plot.setup(18, 72, tft.width() - 43, 50);
    m_plot.setXRange(f[0], f[n - 1]);
    m_plot.setMagRange(false, gLo, gHi);
    m_plot.setPhRange(pLo, pHi);
    m_plot.setLegend("dB", "deg");
    m_plot.drawFrame();
    m_plot.drawCurveMag(f, gainDb, n, ui::C_CH1);
    m_plot.drawCurvePh(f, phaseDeg, n, ui::C_CH2);
}

void TwoPortScreen::drawReady()
{
    const TwoPortDataset* d = sweep.sealedTwoPortDataset();
    char buf[32];
    tft.fillScreen(ui::C_BG);
    ui::topBar("H SWEEP SEALED", false);
    tft.setTextFont(1);
    tft.setTextColor(ui::C_DIM, ui::C_BG);
    if (d) {
        snprintf(buf, sizeof(buf), "points %d err %d", d->nPoints, d->diag.failedPoints);
        tft.drawString(buf, 5, 24);
        snprintf(buf, sizeof(buf), "%luB CRC %08lX", (unsigned long)d->csvLen,
                 (unsigned long)d->crc32);
        tft.drawString(buf, 5, 38);
        tft.drawString("GAIN dB / PHASE deg", 5, 52);
        tft.drawString("H raw ratio", 5, 62);
    } else {
        tft.drawString("NO DATASET", 5, 24);
    }
    drawPreview();
    ui::bottomHint("OK:BLE BACK:CONFIG");
}

void TwoPortScreen::drawBle()
{
    tft.fillScreen(ui::C_BG);
    ui::topBar("BLE UPLOAD H", true);
    ui::bottomHint("BACK:STOP BLE");
    m_bleUiValid = false;
    m_lastBleUiMs = 0;
    updateBle(true);
}

void TwoPortScreen::updateBle(bool force)
{
    const uint32_t now = millis();
    const uint8_t state = (uint8_t)radio.state();
    const uint32_t sent = (uint32_t)radio.bytesSent();
    const uint32_t total = (uint32_t)radio.bytesTotal();
    const bool complete = radio.transferComplete();

    const bool stateChanged = !m_bleUiValid || state != m_bleUiState;
    const bool bytesChanged = !m_bleUiValid || sent != m_bleUiSent || total != m_bleUiTotal;
    const bool doneChanged = !m_bleUiValid || complete != m_bleUiDone;
    if (!stateChanged && !bytesChanged && !doneChanged) return;

    const bool urgent = !m_bleUiValid || stateChanged || doneChanged;
    if (!force && !urgent && (uint32_t)(now - m_lastBleUiMs) < kUiRefreshMinMs)
        return;

    char buf[28];
    tft.setTextFont(1);
    if (stateChanged) {
        tft.fillRect(7, 28, tft.width() - 14, 16, ui::C_BG);
        tft.setTextColor(ui::C_FG, ui::C_BG);
        tft.drawString(radioStateText(radio.state()), 7, 30);
    }
    if (bytesChanged) {
        tft.fillRect(7, 48, tft.width() - 14, 42, ui::C_BG);
        tft.setTextColor(ui::C_FG, ui::C_BG);
        snprintf(buf, sizeof(buf), "%lu/%lu B", (unsigned long)sent,
                 (unsigned long)total);
        tft.drawString(buf, 7, 50);
        ui::progressBar(7, 76, tft.width() - 14, 12,
                        total ? (double)sent / total : 0.0, ui::C_OK);
    }
    if (doneChanged) {
        tft.fillRect(7, 100, tft.width() - 14, 14, ui::C_BG);
        if (complete) {
            tft.setTextColor(ui::C_OK, ui::C_BG);
            tft.drawString("DONE - SITE CAN PLOT", 7, 102);
        }
    }

    m_bleUiState = state;
    m_bleUiSent = sent;
    m_bleUiTotal = total;
    m_bleUiDone = complete;
    m_bleUiValid = true;
    m_lastBleUiMs = now;
}

void TwoPortScreen::onTick()
{
    if (m_phase == Phase::Run) {
        sweep.poll(millis());
        const SweepState st = sweep.state();
        if (st == SweepState::TransferReady) {
            m_phase = Phase::Ready;
            drawReady();
        } else if (st == SweepState::Insufficient) {
            m_errUntilMs = millis() + 4000;
            m_phase = Phase::Config;
            drawConfig();
            tft.setTextFont(1);
            tft.setTextColor(ui::C_ERR, ui::C_BG);
            tft.drawCentreString("DATA INSUFFICIENT <2", tft.width() / 2, 132, 1);
        } else if (st == SweepState::Cancelled || st == SweepState::Error) {
            m_phase = Phase::Config;
            drawConfig();
        } else {
            updateRun(st == SweepState::Stopping);
        }
        return;
    }
    if (m_phase == Phase::Ble) {
        // radio.poll() intentionally only runs once in LCR_UI.loop().
        updateBle();
    }
}

void TwoPortScreen::onEvent(InputEvent e)
{
    if (m_phase == Phase::Run) {
        if (e == InputEvent::Back) sweep.cancel();
        return;
    }
    if (m_phase == Phase::Ready) {
        if (e == InputEvent::Ok) {
            const TwoPortDataset* d = sweep.sealedTwoPortDataset();
            if (d && radio.startBleForSealedDataset(*d)) {
                m_phase = Phase::Ble;
                drawBle();
            }
            return;
        }
        if (e == InputEvent::Back) {
            m_phase = Phase::Config;
            drawConfig();
        }
        return;
    }
    if (m_phase == Phase::Ble) {
        if (e == InputEvent::Back) {
            radio.stopBle();
            m_phase = Phase::Config;
            drawConfig();
        }
        return;
    }

    if (e == InputEvent::Ok) { startSweep(); return; }
    if (e == InputEvent::Back) { screens.pop(); return; }

    DigitEditor* eds[3] = {&m_f0, &m_f1, &m_ppd};
    const int oldField = m_field;
    const bool editorChanged = eds[m_field]->onEvent(e);
    if (!editorChanged) {
        if (e == InputEvent::Down && m_field < 2) { ++m_field; eds[m_field]->setCursor(0); }
        else if (e == InputEvent::Up && m_field > 0) {
            --m_field;
            eds[m_field]->setCursor(m_field < 2 ? 4 : 1);
        }
    }
    if (m_field != oldField) {
        drawConfigField(oldField);
        drawConfigField(m_field);
    } else if (editorChanged) {
        drawConfigField(m_field);
    }
}
