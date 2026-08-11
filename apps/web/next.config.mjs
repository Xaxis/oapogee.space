/** @type {import('next').NextConfig} */
const nextConfig = {
  reactStrictMode: true,
  // Content and data live at the repository root, outside the workspace, because
  // they are the product and the site is one renderer of them. Next needs to be
  // told where the tracing root is or it infers the wrong one in a monorepo.
  outputFileTracingRoot: new URL('../../', import.meta.url).pathname,
  eslint: {
    dirs: ['app', 'components', 'lib'],
  },
}

export default nextConfig
