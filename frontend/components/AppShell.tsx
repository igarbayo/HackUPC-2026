'use client'

import NavBar from '@/components/NavBar'
import Sidebar from '@/components/Sidebar'
import { SimulationStreamProvider, useSimulationStream } from '@/lib/SimulationStreamContext'

function LiveNavBar() {
  const { streams } = useSimulationStream()
  const count = Object.values(streams).filter(
    s => s.status === 'streaming' || s.status === 'connecting'
  ).length
  return <NavBar liveCount={count} />
}

export default function AppShell({ children }: { children: React.ReactNode }) {
  return (
    <SimulationStreamProvider>
      <div style={{ width: '100%', height: '100%', background: '#ffffff' }}>
        <LiveNavBar />
        <Sidebar />
        <div style={{
          position: 'fixed',
          top: 48,
          left: 48,
          right: 0,
          bottom: 0,
          background: '#fff',
          overflowY: 'auto',
        }}>
          {children}
        </div>
      </div>
    </SimulationStreamProvider>
  )
}
