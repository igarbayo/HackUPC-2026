'use client'

import { FF, T } from '@/lib/tokens'

interface CfgInputProps {
  label: string
  value: string
  onChange: (v: string) => void
  placeholder?: string
  type?: string
}

export default function CfgInput({ label, value, onChange, placeholder, type = 'text' }: CfgInputProps) {
  return (
    <div style={{ marginBottom: 14 }}>
      <div style={{ ...T.label, marginBottom: 5 }}>{label}</div>
      <input
        type={type}
        value={value}
        onChange={e => onChange(e.target.value)}
        placeholder={placeholder}
        style={{
          width: '100%', border: 'none', borderBottom: '1px solid #e0e0e0',
          padding: '5px 0', fontSize: 11, fontFamily: FF,
          outline: 'none', letterSpacing: '0.02em', background: 'transparent',
          color: '#000',
        }}
      />
    </div>
  )
}
