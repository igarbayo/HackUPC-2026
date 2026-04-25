import type { Config } from './tokens'

export interface ShuttleData {
  id: string
  aisle: number
  level: number
  status: 'active' | 'idle'
}

export interface RobotData {
  id: string
  aisle: number
  status: 'picking' | 'idle'
  palletCount: number
  throughput: number
}

export interface MockData {
  shuttles: ShuttleData[]
  robots: RobotData[]
  timestamp: number
}

export function makeMockData(cfg: Config): MockData {
  const shuttles: ShuttleData[] = []
  for (let a = 0; a < cfg.aisles; a++) {
    for (let l = 0; l < cfg.levels; l++) {
      shuttles.push({
        id: `SH-A${a + 1}-L${l + 1}`,
        aisle: a,
        level: l,
        status: Math.random() > 0.05 ? 'active' : 'idle',
      })
    }
  }

  const robots: RobotData[] = Array.from({ length: cfg.aisles }, (_, i) => ({
    id: `ROB-${i + 1}`,
    aisle: i,
    status: Math.random() > 0.25 ? 'picking' : 'idle',
    palletCount: Math.floor(Math.random() * 24),
    throughput: Math.floor(Math.random() * 80 + 60),
  }))

  return { shuttles, robots, timestamp: Date.now() }
}
