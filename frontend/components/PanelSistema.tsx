import { FF, T, type Config } from '@/lib/tokens'
import type { MockData } from '@/lib/mock'
import StatRow from './ui/StatRow'
import Hr from './ui/Hr'

interface PanelSistemaProps {
  cfg: Config
  mockData: MockData
}

export default function PanelSistema({ cfg, mockData }: PanelSistemaProps) {
  const activeShuttles = mockData.shuttles.filter(s => s.status === 'active').length
  const totalThruput   = mockData.robots.reduce((s, r) => s + r.throughput, 0)
  const ts = new Date(mockData.timestamp).toLocaleTimeString('es-ES')

  return (
    <div style={{ padding: '28px 24px', fontFamily: FF }}>
      <div style={{ ...T.section, marginBottom: 24, color: '#000' }}>Sistema</div>

      <div style={{ ...T.label, marginBottom: 10 }}>Estructura</div>
      <StatRow label="Pasillos"     value={cfg.aisles} />
      <StatRow label="Niveles"      value={cfg.levels} />
      <StatRow label="Profundidad"  value={`${cfg.depth} slots`} />

      <Hr my={16} />
      <div style={{ ...T.label, marginBottom: 10 }}>Estado en tiempo real</div>
      <StatRow label="Shuttles activos" value={`${activeShuttles} / ${cfg.aisles * cfg.levels}`} accent="#22c55e" />
      <StatRow label="Robots"           value={cfg.aisles} />
      <StatRow label="Throughput"       value={`${totalThruput} /h`} />
      <StatRow label="Ocupación"        value="82%" />

      <Hr my={16} />
      <div style={{ ...T.label, marginBottom: 10 }}>Por pasillo</div>
      {mockData.robots.map((r, i) => (
        <div
          key={r.id}
          style={{
            display: 'flex', justifyContent: 'space-between',
            padding: '4px 0', borderBottom: '1px solid #f8f8f8',
          }}
        >
          <span style={{ ...T.label, color: '#555' }}>A{String(i + 1).padStart(2, '0')}</span>
          <span style={{
            fontSize: 9, letterSpacing: '0.04em',
            color: r.status === 'picking' ? '#22c55e' : '#666',
          }}>
            {r.status === 'picking' ? '● ACTIVO' : '○ IDLE'}
          </span>
          <span style={{ fontSize: 9, color: '#555', letterSpacing: '0.02em' }}>{r.throughput}/h</span>
        </div>
      ))}

      <Hr my={16} />
      <div style={T.micro}>Última actualización: {ts}</div>
    </div>
  )
}
