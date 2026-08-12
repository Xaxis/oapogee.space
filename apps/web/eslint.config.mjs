import { dirname } from 'path'
import { fileURLToPath } from 'url'
import { FlatCompat } from '@eslint/eslintrc'

const compat = new FlatCompat({ baseDirectory: dirname(fileURLToPath(import.meta.url)) })

// next-env.d.ts is written by Next on every build and says at the top that it
// should not be edited, so linting it only ever produces an error nobody may fix.
const config = [
  ...compat.extends('next/core-web-vitals', 'next/typescript'),
  // Both build directories hold generated route types. next dev writes to
  // .next-dev so that a running dev server cannot clobber a production build.
  { ignores: ['.next/**', '.next-dev/**', 'node_modules/**', 'next-env.d.ts'] },
]

export default config
