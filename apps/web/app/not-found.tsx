import Link from 'next/link'

export default function NotFound() {
  return (
    <div className="max-w-[46rem] py-16">
      <p className="font-mono text-xs uppercase tracking-widest text-[var(--color-hivis)]">404</p>
      <h1 className="mt-4 text-3xl font-semibold text-white">This page does not exist yet</h1>
      <p className="mt-4 text-[var(--color-muted)]">
        Most of oApogee is still unwritten. The <Link href="/status">status page</Link> lists every
        planned page and says plainly which ones exist, and the{' '}
        <Link href="/roadmap">page map</Link> explains what each of them will contain.
      </p>
    </div>
  )
}
