// Number / unit formatting helpers for instrument-style readouts.
//
// UI rule: physical values are rendered in engineering notation (powers of
// 10^3 with SI prefixes) rather than scientific e-notation. Axis labels use
// the same formatter as cards/tables so zooming cannot silently switch styles.

const ENG_PREFIX: Record<number, string> = {
  30: 'Q', 27: 'R', 24: 'Y', 21: 'Z', 18: 'E', 15: 'P', 12: 'T',
  9: 'G', 6: 'M', 3: 'k', 0: '',
  [-3]: 'm', [-6]: 'µ', [-9]: 'n', [-12]: 'p', [-15]: 'f',
  [-18]: 'a', [-21]: 'z', [-24]: 'y', [-27]: 'r', [-30]: 'q',
}

function nonFinite(x: number | null | undefined): string | null {
  if (x === null || x === undefined || Number.isNaN(x)) return '—'
  if (x === Infinity) return '∞'
  if (x === -Infinity) return '-∞'
  return null
}

function trimZeros(s: string): string {
  if (!s.includes('.')) return s
  return s.replace(/0+$/, '').replace(/\.$/, '')
}

/** Format a small engineering mantissa without ever producing e-notation. */
function mantissa(value: number, digits: number): string {
  if (value === 0 || Object.is(value, -0)) return '0'
  const d = Math.max(1, Math.min(12, Math.trunc(digits)))
  const order = Math.floor(Math.log10(Math.abs(value)))
  const decimals = Math.max(0, Math.min(12, d - order - 1))
  return trimZeros(value.toFixed(decimals))
}

function engineeringParts(x: number, digits: number): { value: string; prefix: string } {
  if (x === 0 || Object.is(x, -0)) return { value: '0', prefix: '' }

  const ax = Math.abs(x)
  let exponent = Math.floor(Math.log10(ax) / 3) * 3
  exponent = Math.max(-30, Math.min(30, exponent))
  let scaled = x / 10 ** exponent
  let text = mantissa(scaled, digits)

  // Significant-digit rounding can turn 999.99 k into 1000 k. Promote that
  // boundary to 1 M so adjacent ticks remain meaningful and SI-normalized.
  if (Math.abs(Number(text)) >= 1000 && exponent < 30) {
    exponent += 3
    scaled = x / 10 ** exponent
    text = mantissa(scaled, digits)
  }

  return { value: text, prefix: ENG_PREFIX[exponent] ?? '' }
}

/** Dimensionless engineering number, e.g. 0.00047 -> 470µ. */
export function fmt(x: number | null | undefined, digits = 4): string {
  const special = nonFinite(x)
  if (special !== null) return special
  const p = engineeringParts(x as number, digits)
  return `${p.value}${p.prefix}`
}

/**
 * Engineering number with a physical unit. Prefix and unit are inseparable
 * (`kHz`, `mV`, `µA`, `kΩ`); the value and unit are separated by one space.
 */
export function eng(x: number | null | undefined, unit = '', digits = 3): string {
  const special = nonFinite(x)
  if (special !== null) return special
  const p = engineeringParts(x as number, digits)
  return unit ? `${p.value} ${p.prefix}${unit}` : `${p.value}${p.prefix}`
}

/** Compact chart tick/crosshair formatter; axis title carries the base unit. */
export function engAxis(x: number | null | undefined, digits = 6): string {
  return fmt(x, digits)
}

export function deg(rad: number | null | undefined, digits = 2): string {
  if (rad === null || rad === undefined || Number.isNaN(rad)) return '—'
  if (!Number.isFinite(rad)) return rad === -Infinity ? '-∞°' : '∞°'
  return `${(rad * 180 / Math.PI).toFixed(digits)}°`
}

export function degFromDeg(d: number | null | undefined, digits = 2): string {
  if (d === null || d === undefined || Number.isNaN(d)) return '—'
  if (!Number.isFinite(d)) return d === -Infinity ? '-∞°' : '∞°'
  return `${d.toFixed(digits)}°`
}

export function pct(x: number | null | undefined, digits = 2): string {
  if (x === null || x === undefined || Number.isNaN(x)) return '—'
  if (!Number.isFinite(x)) return x === -Infinity ? '-∞%' : '∞%'
  return `${(x * 100).toFixed(digits)}%`
}

export function fmtHz(f: number): string {
  return eng(f, 'Hz', 4)
}

export function fmtTime(t: number): string {
  // seconds
  return eng(t, 's', 3)
}
