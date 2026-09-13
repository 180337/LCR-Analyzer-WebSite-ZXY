// ECharts option builders. Each takes the active palette so charts re-render
// correctly on theme switch. Options are plain objects (typed loosely).

import type { Palette } from './palette'
import { eng, engAxis } from './format'

type AnyOpt = Record<string, any>

function axisStyle(p: Palette, log = false): AnyOpt {
  return {
    type: log ? 'log' : 'value',
    axisLine: { lineStyle: { color: p.baseline } },
    axisTick: { lineStyle: { color: p.baseline } },
    axisLabel: { color: p.text3, fontSize: 11, hideOverlap: true },
    splitLine: { show: true, lineStyle: { color: p.grid } },
  }
}

function pointerLabel(formatter: (v: number) => string): AnyOpt {
  return {
    label: {
      formatter: (params: any) => formatter(Number(params?.value)),
    },
  }
}

function tooltip(p: Palette, trigger: 'axis' | 'item' = 'axis'): AnyOpt {
  const base: AnyOpt = {
    trigger,
    backgroundColor: p.surface,
    borderColor: p.border,
    borderWidth: 1,
    textStyle: { color: p.text, fontSize: 12 },
  }
  if (trigger === 'axis') {
    base.axisPointer = {
      type: 'cross',
      lineStyle: { color: p.text3, width: 1, type: 'dashed' },
      label: { color: p.text, backgroundColor: p.surface, fontSize: 11 },
    }
  }
  return base
}

function grid(): AnyOpt {
  return { left: 60, right: 18, top: 28, bottom: 38 }
}

function degreeTick(v: number): string {
  if (!Number.isFinite(v)) return v === -Infinity ? '-∞°' : '∞°'
  const text = v.toFixed(2).replace(/\.0+$|(?<=\.[0-9])0$/, '').replace(/\.$/, '')
  return `${text}°`
}

function zoomSlider(p: Palette): AnyOpt {
  return {
    type: 'slider', xAxisIndex: 0, height: 14, bottom: 6,
    borderColor: p.border, fillerColor: 'rgba(36, 86, 166, 0.08)',
    handleStyle: { color: p.surface, borderColor: p.baseline },
    textStyle: { color: p.text3, fontSize: 10 },
    labelFormatter: (value: number) => engAxis(value),
  }
}

export interface LineSeries {
  name: string
  data: [number, number][]     // [x, y]
  color: string
  kind?: 'scatter' | 'line'
  dashed?: boolean
  width?: number
  markLine?: AnyOpt
}

export function waveformOpt(p: Palette, opts: {
  series: LineSeries[]
  xLabel?: string
  yLabel?: string
  xFormatter?: (v: number) => string
  yFormatter?: (v: number) => string
}): AnyOpt {
  const multi = opts.series.length > 1
  const xFmt = opts.xFormatter ?? ((v: number) => engAxis(v))
  const yFmt = opts.yFormatter ?? ((v: number) => engAxis(v))
  return {
    animation: false,
    grid: grid(),
    legend: multi
      ? { show: true, textStyle: { color: p.text2, fontSize: 11 }, top: 0, itemWidth: 14, itemHeight: 3, icon: 'roundRect' }
      : { show: false },
    tooltip: {
      ...tooltip(p, 'axis'),
      valueFormatter: (v: any) => (typeof v === 'number' ? engAxis(v) : v),
    },
    xAxis: {
      ...axisStyle(p),
      name: opts.xLabel, nameLocation: 'middle', nameGap: 26,
      nameTextStyle: { color: p.text3, fontSize: 11 },
      axisLabel: { ...axisStyle(p).axisLabel, formatter: xFmt },
      axisPointer: pointerLabel(xFmt),
    },
    yAxis: {
      ...axisStyle(p),
      name: opts.yLabel, nameLocation: 'middle', nameGap: 44,
      nameTextStyle: { color: p.text3, fontSize: 11 },
      scale: true,
      axisLabel: { ...axisStyle(p).axisLabel, formatter: yFmt },
      axisPointer: pointerLabel(yFmt),
    },
    series: opts.series.map((s) => {
      const isScatter = s.kind === 'scatter'
      return {
        type: isScatter ? 'scatter' : 'line',
        name: s.name,
        data: s.data,
        color: s.color,
        symbolSize: isScatter ? 2.6 : 0,
        showSymbol: isScatter,
        lineStyle: isScatter ? undefined : { width: s.width ?? 2, type: s.dashed ? 'dashed' : 'solid' },
        itemStyle: isScatter ? { opacity: 0.55 } : undefined,
        smooth: false,
        z: isScatter ? 2 : 3,
        markLine: s.markLine,
      }
    }),
  }
}

export function spectrumOpt(p: Palette, opts: {
  series: { name: string; freqs: number[]; mag: number[]; color: string; area?: boolean }[]
  excitation?: number
  xLabel?: string
  yLabel?: string
}): AnyOpt {
  const minF = Math.min(...opts.series.flatMap((s) => s.freqs).filter((f) => f > 0), 1)
  return {
    animation: false,
    grid: grid(),
    legend: opts.series.length > 1
      ? { show: true, textStyle: { color: p.text2, fontSize: 11 }, top: 0, itemWidth: 14, itemHeight: 3, icon: 'roundRect' }
      : { show: false },
    tooltip: {
      ...tooltip(p, 'axis'),
      valueFormatter: (v: any) => (typeof v === 'number' ? engAxis(v) : v),
    },
    xAxis: {
      ...axisStyle(p, true),
      min: minF,
      name: opts.xLabel || '频率 (Hz)', nameLocation: 'middle', nameGap: 26,
      nameTextStyle: { color: p.text3, fontSize: 11 },
      axisLabel: { ...axisStyle(p).axisLabel, formatter: (v: number) => engAxis(v) },
      axisPointer: pointerLabel((v) => engAxis(v)),
    },
    yAxis: {
      ...axisStyle(p),
      name: opts.yLabel || '幅值', nameLocation: 'middle', nameGap: 44,
      nameTextStyle: { color: p.text3, fontSize: 11 },
      axisLabel: { ...axisStyle(p).axisLabel, formatter: (v: number) => engAxis(v) },
      axisPointer: pointerLabel((v) => engAxis(v)),
    },
    series: opts.series.map((s) => {
      const data = s.freqs.map((f, i) => [f, s.mag[i]]) as [number, number][]
      const ml = opts.excitation
        ? {
            markLine: {
              silent: true,
              symbol: 'none',
              lineStyle: { color: p.text3, type: 'dashed', width: 1 },
              label: { color: p.text3, fontSize: 10, formatter: 'f₀' },
              data: [{ xAxis: opts.excitation }],
            },
          }
        : {}
      return {
        type: 'line', name: s.name, data, color: s.color,
        showSymbol: false, smooth: true,
        lineStyle: { width: 1.8 },
        areaStyle: s.area ? { opacity: 0.12 } : undefined,
        ...ml,
      }
    }),
  }
}

export function bodeOpt(p: Palette, opts: {
  mode: 'mag' | 'phase'
  measured: { f: number; v: number; sigma?: number }[]
  theory?: { f: number[]; v: number[] }
  yLabel: string
  zoom?: boolean
  showSigma?: boolean
}): AnyOpt {
  const measData = opts.measured.map((m) => [m.f, m.v]) as [number, number][]
  const series: AnyOpt[] = [
    {
      type: 'scatter', name: '测量', data: measData,
      color: p.series[0], symbolSize: 7,
      itemStyle: { color: p.series[0], borderColor: 'transparent' },
      z: 4,
    },
  ]
  // 1-sigma error bars via a custom series rendering a vertical bar per point
  if (opts.showSigma && opts.measured.some((m) => (m.sigma ?? 0) > 0)) {
    const bars = opts.measured.filter((m) => (m.sigma ?? 0) > 0)
    series.push({
      type: 'custom', name: '±1σ', silent: true, z: 3,
      renderItem: (_params: any, api: any) => {
        const x = api.coord([api.value(0), api.value(1)])[0]
        const yLo = api.coord([api.value(0), api.value(1) - api.value(2)])[1]
        const yHi = api.coord([api.value(0), api.value(1) + api.value(2)])[1]
        return {
          type: 'group',
          children: [
            { type: 'line', shape: { x1: x, y1: yLo, x2: x, y2: yHi },
              style: { stroke: p.text3, lineWidth: 1 }, silent: true },
          ],
        }
      },
      data: bars.map((m) => [m.f, m.v, m.sigma]),
      itemStyle: { color: p.text3 },
    })
  }
  if (opts.theory) {
    series.push({
      type: 'line', name: '拟合模型', data: opts.theory.f.map((f, i) => [f, opts.theory!.v[i]]),
      color: p.series[1], showSymbol: false, smooth: true,
      lineStyle: { width: 2 }, z: 3,
    })
  }
  const minF = Math.min(...measData.map((d) => d[0]).filter((f) => f > 0), 1)
  const yFmt = opts.mode === 'phase' ? degreeTick : (v: number) => engAxis(v)
  const base: AnyOpt = {
    animation: false,
    grid: grid(),
    legend: { show: true, textStyle: { color: p.text2, fontSize: 11 }, top: 0, itemWidth: 14, itemHeight: 3, icon: 'roundRect' },
    tooltip: {
      ...tooltip(p, 'axis'),
      valueFormatter: (v: any) => typeof v === 'number' ? yFmt(v) : v,
    },
    xAxis: {
      ...axisStyle(p, true), min: minF,
      name: '频率 (Hz)', nameLocation: 'middle', nameGap: 26,
      nameTextStyle: { color: p.text3, fontSize: 11 },
      axisLabel: { ...axisStyle(p).axisLabel, formatter: (v: number) => engAxis(v) },
      axisPointer: pointerLabel((v) => engAxis(v)),
    },
    yAxis: {
      ...axisStyle(p), name: opts.yLabel, nameLocation: 'middle', nameGap: 48,
      nameTextStyle: { color: p.text3, fontSize: 11 },
      scale: true,
      axisLabel: { ...axisStyle(p).axisLabel, formatter: yFmt },
      axisPointer: pointerLabel(yFmt),
    },
    series,
  }
  if (opts.zoom) {
    base.dataZoom = [
      { type: 'inside', xAxisIndex: 0 },
      zoomSlider(p),
    ]
    base.grid = { left: 60, right: 18, top: 28, bottom: 56 }
  }
  return base
}

export function nyquistOpt(p: Palette, opts: {
  measured: { re: number; im: number }[]
  theory?: { re: number[]; im: number[] }
  zoom?: boolean
  xLabel?: string
  yLabel?: string
}): AnyOpt {
  // Plot -Im on y (electrochemistry convention) so capacitive Z (Im<0) is upper half.
  // Labels are configurable because this builder is also used for dimensionless H.
  const measData = opts.measured.map((m) => [m.re, -m.im]) as [number, number][]
  const series: AnyOpt[] = [
    {
      type: 'scatter', name: '测量', data: measData, color: p.series[0],
      symbolSize: 7, itemStyle: { color: p.series[0] }, z: 4,
    },
  ]
  if (opts.theory) {
    series.push({
      type: 'line', name: '拟合模型',
      data: opts.theory.re.map((r, i) => [r, -opts.theory!.im[i]]),
      color: p.series[1], showSymbol: false, smooth: true,
      lineStyle: { width: 2 }, z: 3,
    })
  }
  const base: AnyOpt = {
    animation: false,
    grid: { left: 60, right: 18, top: 28, bottom: 38 },
    legend: { show: true, textStyle: { color: p.text2, fontSize: 11 }, top: 0, itemWidth: 14, itemHeight: 3, icon: 'roundRect' },
    tooltip: {
      ...tooltip(p, 'axis'),
      valueFormatter: (v: any) => (typeof v === 'number' ? engAxis(v) : v),
    },
    xAxis: {
      ...axisStyle(p), name: opts.xLabel ?? 'Re(Z) (Ω)', nameLocation: 'middle', nameGap: 28,
      nameTextStyle: { color: p.text3, fontSize: 11 },
      axisLabel: { ...axisStyle(p).axisLabel, formatter: (v: number) => engAxis(v) },
      axisPointer: pointerLabel((v) => engAxis(v)),
      scale: true,
    },
    yAxis: {
      ...axisStyle(p), name: opts.yLabel ?? '−Im(Z) (Ω)', nameLocation: 'middle', nameGap: 44,
      nameTextStyle: { color: p.text3, fontSize: 11 },
      axisLabel: { ...axisStyle(p).axisLabel, formatter: (v: number) => engAxis(v) },
      axisPointer: pointerLabel((v) => engAxis(v)),
      scale: true,
    },
    series,
  }
  if (opts.zoom) {
    base.dataZoom = [
      { type: 'inside', xAxisIndex: 0 }, { type: 'inside', yAxisIndex: 0 },
      zoomSlider(p),
    ]
    base.grid = { left: 60, right: 18, top: 28, bottom: 56 }
  }
  return base
}

/** s-plane pole-zero map: poles = ×, zeros = ○, unstable half tinted. */
export function poleZeroOpt(p: Palette, opts: {
  poles: number[][]            // [re, im]
  zeros?: number[][]
  band?: [number, number]      // measured band in rad/s, drawn as horizontal guides
}): AnyOpt {
  const xs = [...opts.poles, ...(opts.zeros ?? [])].map((z) => z[0])
  const ys = [...opts.poles, ...(opts.zeros ?? [])].map((z) => Math.abs(z[1]))
  const rx = Math.max(...xs.map(Math.abs), (opts.band ? opts.band[1] : 1) * 0.1, 1) * 1.25
  const ry = Math.max(...ys, (opts.band ? opts.band[1] : 1) * 0.1, 1) * 1.25
  return {
    animation: false,
    grid: { left: 60, right: 18, top: 28, bottom: 38 },
    legend: {
      show: true, textStyle: { color: p.text2, fontSize: 11 }, top: 0,
      itemWidth: 12, itemHeight: 8,
      data: [
        { name: '极点 ×', icon: 'pin' },
        { name: '零点 ○', icon: 'circle' },
      ],
    },
    tooltip: {
      ...tooltip(p, 'item'),
      valueFormatter: (v: any) => (typeof v === 'number' ? engAxis(v) : v),
    },
    xAxis: {
      ...axisStyle(p), min: -rx, max: rx,
      name: 'Re(s) (rad/s)', nameLocation: 'middle', nameGap: 28,
      nameTextStyle: { color: p.text3, fontSize: 11 },
      axisLabel: { ...axisStyle(p).axisLabel, formatter: (v: number) => engAxis(v) },
      axisPointer: pointerLabel((v) => engAxis(v)),
    },
    yAxis: {
      ...axisStyle(p), min: -ry, max: ry,
      name: 'Im(s) (rad/s)', nameLocation: 'middle', nameGap: 44,
      nameTextStyle: { color: p.text3, fontSize: 11 },
      axisLabel: { ...axisStyle(p).axisLabel, formatter: (v: number) => engAxis(v) },
      axisPointer: pointerLabel((v) => engAxis(v)),
    },
    series: [
      {
        type: 'scatter', name: '极点 ×', symbol: 'triangle', symbolSize: 9,
        symbolRotate: 180,
        data: opts.poles.map((z) => [z[0], z[1]]),
        itemStyle: { color: p.series[1] },
      },
      {
        type: 'scatter', name: '零点 ○', symbol: 'circle', symbolSize: 8,
        data: (opts.zeros ?? []).map((z) => [z[0], z[1]]),
        itemStyle: { color: 'transparent', borderColor: p.series[0], borderWidth: 1.5 },
      },
      {
        type: 'line', silent: true, data: [[0, -ry], [0, ry]],
        lineStyle: { color: p.baseline, type: 'dashed', width: 1 },
        showSymbol: false, tooltip: { show: false }, z: 1,
      },
    ],
  }
}

/** Weighted residual vs frequency, with the ±1σ reference band. */
export function residualsOpt(p: Palette, opts: {
  freqs: number[]
  re: number[]
  im: number[]
  sigma?: number[]        // per-point 1σ (same units as re/im); draws ±1σ band
}): AnyOpt {
  const series: AnyOpt[] = [
    {
      type: 'scatter', name: 'Re 残差', data: opts.freqs.map((f, i) => [f, opts.re[i]]),
      color: p.series[0], symbolSize: 5, itemStyle: { color: p.series[0] }, z: 4,
    },
    {
      type: 'scatter', name: 'Im 残差', data: opts.freqs.map((f, i) => [f, opts.im[i]]),
      color: p.series[3], symbolSize: 5, itemStyle: { color: p.series[3] }, z: 4,
    },
  ]
  if (opts.sigma && opts.sigma.some((s) => s > 0)) {
    const sigma = opts.sigma
    const band = opts.freqs.map((f, i) => [f, sigma[i] ?? 0])
    series.push(
      {
        type: 'line', name: '+1σ', data: band, color: p.text3, showSymbol: false,
        lineStyle: { width: 1, type: 'dashed' }, z: 2,
      },
      {
        type: 'line', name: '−1σ', data: band.map(([f, s]) => [f, -s]),
        color: p.text3, showSymbol: false,
        lineStyle: { width: 1, type: 'dashed' }, z: 2,
      },
    )
  }
  const minF = Math.min(...opts.freqs.filter((f) => f > 0), 1)
  return {
    animation: false,
    grid: grid(),
    legend: { show: true, textStyle: { color: p.text2, fontSize: 11 }, top: 0, itemWidth: 14, itemHeight: 3, icon: 'roundRect' },
    tooltip: {
      ...tooltip(p, 'axis'),
      valueFormatter: (v: any) => (typeof v === 'number' ? engAxis(v) : v),
    },
    xAxis: {
      ...axisStyle(p, true), min: minF,
      name: '频率 (Hz)', nameLocation: 'middle', nameGap: 26,
      nameTextStyle: { color: p.text3, fontSize: 11 },
      axisLabel: { ...axisStyle(p).axisLabel, formatter: (v: number) => engAxis(v) },
      axisPointer: pointerLabel((v) => engAxis(v)),
    },
    yAxis: {
      ...axisStyle(p), name: '残差 (Ω)', nameLocation: 'middle', nameGap: 48,
      nameTextStyle: { color: p.text3, fontSize: 11 },
      axisLabel: { ...axisStyle(p).axisLabel, formatter: (v: number) => engAxis(v) },
      axisPointer: pointerLabel((v) => engAxis(v)),
    },
    series,
  }
}

export function complexPointOpt(p: Palette, z: { re: number; im: number }): AnyOpt {
  return {
    animation: false,
    grid: { left: 50, right: 18, top: 16, bottom: 30 },
    tooltip: { ...tooltip(p, 'item'), formatter: () => `Re=${eng(z.re, 'Ω', 5)}<br/>Im=${eng(z.im, 'Ω', 5)}` },
    xAxis: {
      ...axisStyle(p), name: 'Re (Ω)', nameLocation: 'middle', nameGap: 24,
      nameTextStyle: { color: p.text3, fontSize: 11 },
      axisLabel: { ...axisStyle(p).axisLabel, formatter: (v: number) => engAxis(v) },
      axisPointer: pointerLabel((v) => engAxis(v)),
      scale: true,
    },
    yAxis: {
      ...axisStyle(p), name: 'Im (Ω)', nameLocation: 'middle', nameGap: 36,
      nameTextStyle: { color: p.text3, fontSize: 11 },
      axisLabel: { ...axisStyle(p).axisLabel, formatter: (v: number) => engAxis(v) },
      axisPointer: pointerLabel((v) => engAxis(v)),
      scale: true,
    },
    series: [
      {
        type: 'scatter', data: [[z.re, z.im]], color: p.series[0],
        symbolSize: 12, itemStyle: { color: p.series[0] },
        markLine: {
          silent: true, symbol: 'none',
          lineStyle: { color: p.baseline, type: 'dashed', width: 1 },
          data: [{ yAxis: 0 }, { xAxis: 0 }],
        },
      },
    ],
  }
}
