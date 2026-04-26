import Link from 'next/link'
import Image from 'next/image'
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
      <Link href="/" style={{
        position: 'absolute',
        left: '50%',
        transform: 'translateX(-50%)',
        textDecoration: 'none',
        display: 'flex', alignItems: 'center',
      }}>
        <Image src="/logo.png" alt="XEITECH" height={32} width={120} style={{ objectFit: 'contain' }} />
      </Link>

      {/* Right: live indicator */}
      <div style={{
        marginLeft: 'auto', paddingRight: 28,
        display: 'flex', alignItems: 'center', gap: 6,
        ...T.nav, color: '#555',
      }}>
        <span style={{
          width: 6, height: 6, borderRadius: '50%',
          background: liveCount > 0 ? '#22c55e' : '#ef4444',
          display: 'inline-block', animation: 'pulse 2.4s infinite',
        }} />
        {liveCount} active
      </div>
    </div>
  )
}
