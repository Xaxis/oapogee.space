import { DocsShell } from '@/components/DocsShell'

// A route group, so every page inside keeps its flat URL. /safety is still
// /safety; it simply gains the sidebar, the breadcrumb and the reading order.
// The homepage sits outside this group deliberately: it is the one page whose
// job is to orient somebody who does not yet know the site has a structure.
export default function DocsLayout({ children }: { children: React.ReactNode }) {
  return <DocsShell>{children}</DocsShell>
}
