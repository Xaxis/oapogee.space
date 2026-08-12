/** @type {import('next').NextConfig} */
const nextConfig = {
  reactStrictMode: true,
  // `next dev` and `next build` share an output directory by default, so a dev
  // server left running while `make check` runs rewrites the build's manifests
  // underneath it. The failures that produces are baffling: a stylesheet that
  // 404s, or a page the build swears does not exist. Separate directories make
  // the two independent.
  distDir: process.env.NODE_ENV === 'development' ? '.next-dev' : '.next',
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
