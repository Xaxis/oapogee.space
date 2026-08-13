import Link from 'next/link'

export default function NotFound() {
  return (
    // main sets no width or gutter, so this page supplies the same outer
    // container the header and the footer use and keeps the reading measure on
    // an inner element. Centring the 46rem block instead would leave its left
    // edge hundreds of pixels inboard of the wordmark above it.
    <div className="mx-auto max-w-[1500px] px-5 py-16 lg:px-8">
      <div className="max-w-[46rem]">
        <p className="font-mono text-xs uppercase tracking-widest text-[var(--color-hivis)]">404</p>
        <h1 className="mt-4 text-3xl font-semibold text-white">This page does not exist yet</h1>
        <p className="mt-4 text-[var(--color-muted)]">
          Most of oApogee is still unwritten, so this may be a page that has not been written
          rather than a broken link. The navigation lists everything that exists.
        </p>
      </div>
    </div>
  )
}
