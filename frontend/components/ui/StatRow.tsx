import { T } from '@/lib/tokens'

interface StatRowProps {
  label: string
  value: string | number
  accent?: string
}

export default function StatRow({ label, value, accent }: StatRowProps) {
  return (
    <div style={{ display: 'flex', justifyContent: 'space-between', padding: '4px 0' }}>
      <span style={T.label}>{label}</span>
      <span style={{ ...T.value, color: accent || '#000' }}>{value}</span>
    </div>
  )
}
