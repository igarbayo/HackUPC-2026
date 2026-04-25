'use client'

import { useState } from 'react'
import { FF, T, type Config } from '@/lib/tokens'
import CfgInput from './ui/CfgInput'
import CfgSlider from './ui/CfgSlider'
import CfgToggle from './ui/CfgToggle'
import Hr from './ui/Hr'

interface PanelConfiguracionProps {
  cfg: Config
  setCfg: (cfg: Config) => void
  apiUrl: string
  setApiUrl: (url: string) => void
}

export default function PanelConfiguracion({ cfg, setCfg, apiUrl, setApiUrl }: PanelConfiguracionProps) {
  const [localCfg, setLocalCfg] = useState({ ...cfg })
  const [token, setToken]       = useState('')
  const [pollSec, setPollSec]   = useState(3)
  const [autoRot, setAutoRot]   = useState(true)
  const [showGrid, setShowGrid] = useState(true)
  const [fogOn, setFogOn]       = useState(true)
  const [applied, setApplied]   = useState(false)

  function handleApply() {
    setCfg(localCfg)
    setApplied(true)
    setTimeout(() => setApplied(false), 2000)
  }

  return (
    <div style={{ padding: '28px 24px', fontFamily: FF, overflowY: 'auto', height: '100%' }}>
      <div style={{ ...T.section, marginBottom: 24, color: '#000' }}>Configuración</div>

      {/* Conexión API */}
      <div style={{ ...T.label, color: '#000', marginBottom: 14 }}>Conexión API</div>
      <CfgInput
        label="Endpoint"
        value={apiUrl}
        onChange={setApiUrl}
        placeholder="https://api.example.com/v1/silo"
      />
      <CfgInput
        label="Token de autenticación"
        value={token}
        onChange={setToken}
        type="password"
        placeholder="Bearer ···"
      />
      <CfgSlider label="Intervalo de polling" value={pollSec} onChange={setPollSec} min={1} max={30} unit="s" />

      <div style={{
        display: 'flex', alignItems: 'center', gap: 7,
        padding: '8px 10px', background: '#f8f8f8',
        marginBottom: 4,
      }}>
        <div style={{
          width: 6, height: 6, borderRadius: '50%',
          background: apiUrl ? '#22c55e' : '#e0e0e0',
        }} />
        <span style={{ fontSize: 9, letterSpacing: '0.1em', textTransform: 'uppercase', color: '#888' }}>
          {apiUrl ? 'Conectado a API externa' : 'Usando datos simulados'}
        </span>
      </div>

      <Hr my={20} />

      {/* Estructura */}
      <div style={{ ...T.label, color: '#000', marginBottom: 14 }}>Estructura del silo</div>
      <CfgSlider
        label="Pasillos"
        value={localCfg.aisles}
        onChange={v => setLocalCfg(p => ({ ...p, aisles: v }))}
        min={2} max={12}
      />
      <CfgSlider
        label="Niveles"
        value={localCfg.levels}
        onChange={v => setLocalCfg(p => ({ ...p, levels: v }))}
        min={2} max={8}
      />
      <CfgSlider
        label="Profundidad"
        value={localCfg.depth}
        onChange={v => setLocalCfg(p => ({ ...p, depth: v }))}
        min={6} max={24}
      />

      <button
        onClick={handleApply}
        style={{
          width: '100%', padding: '10px 0', marginTop: 4, marginBottom: 4,
          background: applied ? '#22c55e' : '#000', color: '#fff',
          border: 'none', fontSize: 9, letterSpacing: '0.18em',
          textTransform: 'uppercase', fontFamily: FF,
          cursor: 'pointer', transition: 'background 0.3s',
        }}
      >
        {applied ? '✓ Aplicado' : 'Aplicar y recargar'}
      </button>

      <Hr my={20} />

      {/* Visualización */}
      <div style={{ ...T.label, color: '#000', marginBottom: 14 }}>Opciones de visualización</div>
      <CfgToggle label="Auto-rotación"   value={autoRot}  onChange={setAutoRot}  />
      <CfgToggle label="Grid de suelo"   value={showGrid} onChange={setShowGrid} />
      <CfgToggle label="Niebla de fondo" value={fogOn}    onChange={setFogOn}    />

      <Hr my={20} />
      <div style={T.micro}>v1.0 · SILOS Logistics Viewer</div>
    </div>
  )
}
