export default function Hr({ my = 16 }: { my?: number }) {
  return <div style={{ borderTop: '1px solid #ebebeb', margin: `${my}px 0` }} />
}
