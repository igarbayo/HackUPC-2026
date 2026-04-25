'use client'

import { createContext, useContext, useRef, useState, useCallback } from 'react'

// ── Types ──────────────────────────────────────────────────────────────

interface Position { x: number; y: number; z: number; side: number }

export interface ShuttleState {
  y_level: number
  position: Position
  phase: string
  is_carrying: boolean
  carried_box_id: string
  carried_box_family: string
}

export interface PalletState {
  slot: number
  family: string
  placed_count: number
  reserved_count: number
}

export interface MetricsSnapshot {
  total_pallets_sent: number
  full_pallets: number
  full_pallet_ratio: number
  total_boxes_sent: number
  avg_fill_rate: number
  boxes_by_family: Record<string, number>
}

export interface TickSnapshot {
  tick: number
  aisle: { shuttles: ShuttleState[] }
  robot: { pallets: PalletState[] }
  metrics: MetricsSnapshot
}

export type StreamStatus = 'connecting' | 'streaming' | 'done' | 'error'

export interface StreamState {
  status: StreamStatus
  snapshot: TickSnapshot | null
  totalTicks: number | null
  speed: number
}

interface ContextValue {
  streams: Record<string, StreamState>
  startStream: (simId: string, tps: number) => void
  setSpeed: (simId: string, tps: number) => void
}

// ── Context ────────────────────────────────────────────────────────────

const SimulationStreamContext = createContext<ContextValue>({
  streams: {},
  startStream: () => {},
  setSpeed: () => {},
})

export function useSimulationStream() {
  return useContext(SimulationStreamContext)
}

// ── Provider ───────────────────────────────────────────────────────────

const WS_BASE = (process.env.NEXT_PUBLIC_API_URL ?? 'http://localhost:8000').replace(/^http/, 'ws')

export function SimulationStreamProvider({ children }: { children: React.ReactNode }) {
  const wsMap   = useRef<Map<string, WebSocket>>(new Map())
  const [streams, setStreams] = useState<Record<string, StreamState>>({})

  const patchStream = useCallback((simId: string, patch: Partial<StreamState>) => {
    setStreams(prev => ({
      ...prev,
      [simId]: { ...prev[simId], ...patch },
    }))
  }, [])

  const startStream = useCallback((simId: string, tps: number) => {
    if (wsMap.current.has(simId)) return  // already running

    setStreams(prev => ({
      ...prev,
      [simId]: { status: 'connecting', snapshot: null, totalTicks: null, speed: tps },
    }))

    const ws = new WebSocket(`${WS_BASE}/simulations/${simId}/stream`)
    wsMap.current.set(simId, ws)

    ws.onopen = () => {
      patchStream(simId, { status: 'streaming' })
      ws.send(JSON.stringify({ action: 'set_speed', value: tps }))
    }

    ws.onmessage = (ev: MessageEvent) => {
      try {
        const data = JSON.parse(ev.data as string)
        if (data.type === 'done') {
          patchStream(simId, { status: 'done', totalTicks: data.total_ticks })
          localStorage.setItem(`silos_stream_done_${simId}`, 'true')
          wsMap.current.delete(simId)
        } else if (data.type === 'error') {
          patchStream(simId, { status: 'error' })
          wsMap.current.delete(simId)
        } else {
          patchStream(simId, { snapshot: data as TickSnapshot })
        }
      } catch { /* ignore */ }
    }

    ws.onerror = () => {
      patchStream(simId, { status: 'error' })
      wsMap.current.delete(simId)
    }
  }, [patchStream])

  const setSpeed = useCallback((simId: string, tps: number) => {
    patchStream(simId, { speed: tps })
    const ws = wsMap.current.get(simId)
    if (ws?.readyState === WebSocket.OPEN)
      ws.send(JSON.stringify({ action: 'set_speed', value: tps }))
  }, [patchStream])

  return (
    <SimulationStreamContext.Provider value={{ streams, startStream, setSpeed }}>
      {children}
    </SimulationStreamContext.Provider>
  )
}
