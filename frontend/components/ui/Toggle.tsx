'use client'

interface ToggleProps {
  value: boolean
  onChange: (v: boolean) => void
}

export default function Toggle({ value, onChange }: ToggleProps) {
  return (
    <div
      onClick={() => onChange(!value)}
      style={{
        width: 28, height: 14, position: 'relative',
        background: value ? '#000' : '#e0e0e0',
        cursor: 'pointer', flexShrink: 0,
        transition: 'background 0.2s',
      }}
    >
      <div style={{
        position: 'absolute', top: 2,
        left: value ? 16 : 2,
        width: 10, height: 10,
        background: '#fff',
        transition: 'left 0.2s',
      }} />
    </div>
  )
}
