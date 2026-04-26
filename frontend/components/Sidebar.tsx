'use client'

import Link from 'next/link'
import { usePathname } from 'next/navigation'
import { FF } from '@/lib/tokens'

const ITEMS: { path: string; label: string }[] = [
  { path: '/parameters',  label: 'Parameters'  },
  { path: '/simulations', label: 'Simulations' },
]

export default function Sidebar() {
  const pathname = usePathname()

  return (
    <div style={{
      position: 'fixed', left: 0, top: 48, bottom: 0, width: 48,
      background: '#fff', borderRight: '1px solid #e8e8e8',
      zIndex: 250, display: 'flex', flexDirection: 'column',
      alignItems: 'center', paddingTop: 32, gap: 36,
      fontFamily: FF,
    }}>
      {ITEMS.map(item => {
        const isActive = pathname === item.path
        return (
          <Link
            key={item.path}
            href={item.path}
            title={item.label}
            style={{
              writingMode: 'vertical-rl',
              transform: 'rotate(180deg)',
              fontSize: 9, letterSpacing: '0.18em',
              textTransform: 'uppercase',
              cursor: 'pointer', userSelect: 'none',
              textDecoration: 'none',
              color: isActive ? '#000' : '#bbb',
              borderLeft: isActive ? '1px solid #000' : '1px solid transparent',
              paddingLeft: 5, paddingBottom: 2,
              transition: 'color 0.2s, border-color 0.2s',
            }}
          >
            {item.label}
          </Link>
        )
      })}

      <div style={{ marginTop: 'auto', marginBottom: 20 }}>
        <div style={{ width: 4, height: 4, borderRadius: '50%', background: '#e0e0e0' }} />
      </div>
    </div>
  )
}
