'use client'

import { FF } from '@/lib/tokens'

export type SidebarSection = 'sistema' | 'configuracion' | 'parametros'

const ITEMS: { id: SidebarSection; label: string }[] = [
  { id: 'sistema',       label: 'Sistema'        },
  { id: 'configuracion', label: 'Configuración'  },
  { id: 'parametros',    label: 'Parámetros'     },
]

interface SidebarProps {
  active: SidebarSection | null
  setActive: (id: SidebarSection | null) => void
}

export default function Sidebar({ active, setActive }: SidebarProps) {
  return (
    <div style={{
      position: 'fixed', left: 0, top: 48, bottom: 0, width: 48,
      background: '#fff', borderRight: '1px solid #e8e8e8',
      zIndex: 250, display: 'flex', flexDirection: 'column',
      alignItems: 'center', paddingTop: 32, gap: 36,
      fontFamily: FF,
    }}>
      {ITEMS.map(item => {
        const isActive = active === item.id
        return (
          <div
            key={item.id}
            onClick={() => setActive(isActive ? null : item.id)}
            title={item.label}
            style={{
              writingMode: 'vertical-rl',
              transform: 'rotate(180deg)',
              fontSize: 9, letterSpacing: '0.18em',
              textTransform: 'uppercase',
              cursor: 'pointer', userSelect: 'none',
              color: isActive ? '#000' : '#bbb',
              borderLeft: isActive ? '1px solid #000' : '1px solid transparent',
              paddingLeft: 5, paddingBottom: 2,
              transition: 'color 0.2s, border-color 0.2s',
            }}
          >
            {item.label}
          </div>
        )
      })}

      {/* Bottom version dot */}
      <div style={{ marginTop: 'auto', marginBottom: 20 }}>
        <div style={{ width: 4, height: 4, borderRadius: '50%', background: '#e0e0e0' }} />
      </div>
    </div>
  )
}
