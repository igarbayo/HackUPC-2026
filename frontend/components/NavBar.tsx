import { FF, T } from '@/lib/tokens'

interface NavBarProps {
  liveCount: number
}

export default function NavBar({ liveCount }: NavBarProps) {
  return (
    <div style={{
      position: 'fixed', top: 0, left: 0, right: 0, height: 48,
      display: 'flex', alignItems: 'center',
      background: 'rgba(255,255,255,0.97)',
      backdropFilter: 'blur(10px)',
      borderBottom: '1px solid #e8e8e8',
      zIndex: 300, fontFamily: FF,
    }}>
      {/* Center brand */}
      <div style={{
        ...T.brand,
        position: 'absolute',
        left: '50%',
        transform: 'translateX(-50%)',
        pointerEvents: 'none',
        color: '#000',
      }}>
        SILOS
      </div>

      {/* Right: live indicator */}
      <div style={{
        marginLeft: 'auto', paddingRight: 28,
        display: 'flex', alignItems: 'center', gap: 6,
        ...T.nav, color: '#aaa',
      }}>
        <span style={{
          width: 6, height: 6, borderRadius: '50%', background: '#22c55e',
          display: 'inline-block', animation: 'pulse 2.4s infinite',
        }} />
        {liveCount} active
      </div>
    </div>
  )
}
