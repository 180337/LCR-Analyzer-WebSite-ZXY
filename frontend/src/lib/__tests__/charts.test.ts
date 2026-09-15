import { describe, expect, it } from 'vitest'
import { bodeOpt, nyquistOpt } from '../charts'

const palette = {
  baseline: '#888', text3: '#777', grid: '#ddd', surface: '#fff', border: '#ccc',
  text: '#111', text2: '#444', series: ['#1', '#2', '#3', '#4'],
} as any

describe('chart engineering labels', () => {
  it('uses the shared engineering formatter on axes, including negative values', () => {
    const opt = nyquistOpt(palette, {
      measured: [{ re: -1200, im: 2.2e-6 }],
    })
    expect(opt.xAxis.axisLabel.formatter(-1200)).toBe('-1.2k')
    expect(opt.yAxis.axisLabel.formatter(2.2e-6)).toBe('2.2µ')
    expect(opt.xAxis.axisLabel.formatter(-1200)).not.toContain('--')
  })

  it('keeps pico/femto prefixes on axis ticks, crosshairs and zoom labels', () => {
    const opt = nyquistOpt(palette, {
      measured: [{ re: 8.2e-12, im: -3.3e-15 }],
      zoom: true,
    })
    expect(opt.xAxis.axisLabel.formatter(8.2e-12)).toBe('8.2p')
    expect(opt.yAxis.axisLabel.formatter(3.3e-15)).toBe('3.3f')
    expect(opt.xAxis.axisPointer.label.formatter({ value: 8.2e-12 })).toBe('8.2p')
    expect(opt.dataZoom[2].labelFormatter(8.2e-12)).toBe('8.2p')
  })

  it('does not hard-code ohms onto a generic Bode magnitude axis', () => {
    const opt = bodeOpt(palette, {
      mode: 'mag',
      measured: [{ f: 1000, v: -20 }],
      yLabel: '增益 20log₁₀|H| (dB)',
    })
    expect(opt.xAxis.axisLabel.formatter(1000)).toBe('1k')
    expect(opt.yAxis.axisLabel.formatter(-20)).toBe('-20')
    expect(opt.yAxis.axisLabel.formatter(-20)).not.toContain('Ω')
  })

  it('allows dimensionless H Nyquist labels instead of impedance units', () => {
    const opt = nyquistOpt(palette, {
      measured: [{ re: 0.5, im: -0.5 }],
      xLabel: 'Re(H)',
      yLabel: '−Im(H)',
    })
    expect(opt.xAxis.name).toBe('Re(H)')
    expect(opt.yAxis.name).toBe('−Im(H)')
  })
})
