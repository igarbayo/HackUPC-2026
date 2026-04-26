'use client'

import { T } from '@/lib/tokens'
import Toggle from './Toggle'

interface CfgToggleProps {
  label: string
  value: boolean
  onChange: (v: boolean) => void
}

export default function CfgToggle({ label, value, onChange }: CfgToggleProps) {
  return (
    <div style={{
      display: 'flex', justifyContent: 'space-between', alignItems: 'center',
      paddingBottom: 12, marginBottom: 12, borderBottom: '1px solid #f4f4f4',
    }}>
      <span style={{ ...T.label, color: '#444' }}>{label}</span>
      <Toggle value={value} onChange={onChange} />
    </div>
  )
}
