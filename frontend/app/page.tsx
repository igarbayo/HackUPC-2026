'use client'

import { useState, useEffect } from 'react'
import NavBar from '@/components/NavBar'
import Sidebar, { type SidebarSection } from '@/components/Sidebar'
import PanelSistema from '@/components/PanelSistema'
import PanelConfiguracion from '@/components/PanelConfiguracion'
import PanelParametros from '@/components/PanelParametros'
import { DEFAULT_CONFIG, type Config } from '@/lib/tokens'
import { makeMockData, type MockData } from '@/lib/mock'

export default function Home() {
  const [activeSection, setActiveSection] = useState<SidebarSection | null>(null)
  const [cfg, setCfg]       = useState<Config>(DEFAULT_CONFIG)
  const [apiUrl, setApiUrl] = useState('')
  const [mockData, setMockData] = useState<MockData>(() => makeMockData(DEFAULT_CONFIG))

  useEffect(() => {
    const id = setInterval(() => {
      if (!apiUrl) {
        setMockData(makeMockData(cfg))
        return
      }
      fetch(apiUrl)
        .then(r => r.json())
        .then((d: MockData) => setMockData(d))
        .catch(() => setMockData(makeMockData(cfg)))
    }, 3000)
    return () => clearInterval(id)
  }, [apiUrl, cfg])

  const liveCount = mockData.shuttles.filter(s => s.status === 'active').length

  return (
    <div style={{ width: '100%', height: '100%', background: '#ffffff' }}>
      <NavBar liveCount={liveCount} />
      <Sidebar active={activeSection} setActive={setActiveSection} />

      {/* Main panel area */}
      <div style={{
        position: 'fixed',
        top: 48,
        left: 48,
        right: 0,
        bottom: 0,
        background: '#fff',
        overflowY: 'auto',
        display: activeSection ? 'block' : 'none',
      }}>
        {activeSection === 'sistema' && (
          <PanelSistema cfg={cfg} mockData={mockData} />
        )}
        {activeSection === 'configuracion' && (
          <PanelConfiguracion
            cfg={cfg}
            setCfg={setCfg}
            apiUrl={apiUrl}
            setApiUrl={setApiUrl}
          />
        )}
        {activeSection === 'parametros' && (
          <PanelParametros />
        )}
      </div>
    </div>
  )
}
