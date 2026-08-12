import { ImageResponse } from 'next/og'

// The card people see when a link to this project is pasted into a chat. It
// carries the two things worth knowing before clicking: what oApogee is, and
// that it is a design rather than a finished product. A share card that implies
// a shipping product would be the same lie as a fabricated flight graph, in a
// place that is harder to correct.

export const alt = 'oApogee: open source rocket telemetry'
export const size = { width: 1200, height: 630 }
export const contentType = 'image/png'

export default function OpengraphImage() {
  return new ImageResponse(
    (
      <div
        style={{
          width: '100%',
          height: '100%',
          display: 'flex',
          flexDirection: 'column',
          justifyContent: 'space-between',
          background: '#08090a',
          padding: 72,
          fontFamily: 'sans-serif',
        }}
      >
        <div style={{ display: 'flex', flexDirection: 'column' }}>
          <div
            style={{
              display: 'flex',
              fontSize: 22,
              letterSpacing: 4,
              textTransform: 'uppercase',
              color: '#ffcf1b',
            }}
          >
            Open source rocket telemetry
          </div>
          <div style={{ display: 'flex', marginTop: 28, fontSize: 68, fontWeight: 700 }}>
            <span style={{ color: '#ffcf1b' }}>o</span>
            <span style={{ color: '#ffffff' }}>apogee</span>
          </div>
          <div
            style={{
              display: 'flex',
              marginTop: 24,
              fontSize: 34,
              lineHeight: 1.35,
              color: '#dcdee2',
              maxWidth: 900,
            }}
          >
            A small, cheap sensor package that tells you exactly what your model rocket did.
          </div>
        </div>

        <div
          style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            borderTop: '1px solid #26292e',
            paddingTop: 28,
            fontSize: 24,
            color: '#8b9099',
          }}
        >
          <div style={{ display: 'flex' }}>Solo . Link . Track</div>
          <div style={{ display: 'flex', color: '#ff7a1a' }}>Design stage, nothing has flown</div>
          <div style={{ display: 'flex' }}>oapogee.space</div>
        </div>
      </div>
    ),
    size
  )
}
