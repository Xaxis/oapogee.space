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
  images: {
    // The only SVG the optimiser will ever see is the schematic this repository
    // generates into public/. No remote patterns are configured, so nothing
    // off-origin can reach the optimiser at all.
    dangerouslyAllowSVG: true,
    contentSecurityPolicy: "default-src 'self'; script-src 'none'; sandbox;",
  },
}

export default nextConfig
