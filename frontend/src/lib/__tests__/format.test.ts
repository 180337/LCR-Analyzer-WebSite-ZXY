import { describe, expect, it } from 'vitest'
import { eng, engAxis, fmt, fmtHz } from '../format'

describe('engineering-number formatting', () => {
  it('uses SI engineering prefixes with prefix attached to the unit', () => {
    expect(eng(1500, 'Hz', 4)).toBe('1.5 kHz')
    expect(eng(2.2e-6, 'F', 3)).toBe('2.2 µF')
    expect(eng(-4.7e-9, 'A', 3)).toBe('-4.7 nA')
    expect(eng(8.2e-12, 'F', 3)).toBe('8.2 pF')
    expect(eng(3.3e3, 'Ω', 3)).toBe('3.3 kΩ')
    expect(fmtHz(1e6)).toBe('1 MHz')
  })

  it('covers every engineering SI prefix from quecto through quetta', () => {
    const cases: Array<[number, string]> = [
      [1e-30, '1q'], [1e-27, '1r'], [1e-24, '1y'], [1e-21, '1z'],
      [1e-18, '1a'], [1e-15, '1f'], [1e-12, '1p'], [1e-9, '1n'],
      [1e-6, '1µ'], [1e-3, '1m'], [1, '1'], [1e3, '1k'],
      [1e6, '1M'], [1e9, '1G'], [1e12, '1T'], [1e15, '1P'],
      [1e18, '1E'], [1e21, '1Z'], [1e24, '1Y'], [1e27, '1R'], [1e30, '1Q'],
    ]
    for (const [value, expected] of cases) {
      expect(fmt(value, 5)).toBe(expected)
      expect(engAxis(value, 5)).toBe(expected)
    }
  })

  it('never falls back to scientific notation for instrument-scale values', () => {
    const values = [1e30, 1e24, 1e18, 1e12, 1e9, 1e6, 1e3, 1, 1e-3, 1e-6, 1e-9, 1e-12, 1e-15, 1e-18, 1e-24, 1e-30]
    for (const value of values) {
      expect(fmt(value, 5)).not.toMatch(/[eE][+-]?\d/)
      expect(eng(value, 'V', 5)).not.toMatch(/[eE][+-]?\d/)
    }
  })

  it('keeps negative signs correct and promotes rounded 1000-prefix boundaries', () => {
    expect(engAxis(-1200)).toBe('-1.2k')
    expect(eng(999999, 'Hz', 4)).toBe('1 MHz')
    expect(fmt(-Infinity)).toBe('-∞')
  })

  it('keeps nearby axis ticks distinguishable instead of collapsing to repeated 1 labels', () => {
    const labels = [1.001, 1.002, 1.003].map((v) => engAxis(v))
    expect(new Set(labels).size).toBe(labels.length)
    expect(labels).toEqual(['1.001', '1.002', '1.003'])
  })
})
