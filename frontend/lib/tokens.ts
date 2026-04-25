import type { CSSProperties } from 'react'

export const FF = "'Helvetica Neue', Helvetica, Arial, sans-serif"

export const T: Record<string, CSSProperties> = {
  nav:     { fontSize: 11, letterSpacing: '0.12em', textTransform: 'uppercase', fontWeight: 400 },
  brand:   { fontSize: 15, letterSpacing: '0.26em', textTransform: 'uppercase', fontWeight: 500 },
  label:   { fontSize: 9,  letterSpacing: '0.16em', textTransform: 'uppercase', color: '#555' },
  value:   { fontSize: 11, letterSpacing: '0.04em', fontWeight: 500, color: '#000' },
  micro:   { fontSize: 9,  letterSpacing: '0.12em', textTransform: 'uppercase', color: '#777' },
  section: { fontSize: 9,  letterSpacing: '0.18em', textTransform: 'uppercase', fontWeight: 500 },
}

export const DEFAULT_CONFIG = {
  aisles: 6,
  levels: 5,
  depth: 14,
}

export type Config = typeof DEFAULT_CONFIG
