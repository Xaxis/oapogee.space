import { readFileSync } from 'node:fs'
import { join } from 'node:path'
import { parse } from 'yaml'
import { DATA_DIR, REPO_ROOT } from './repo'

function load<T>(name: string): T {
  return parse(readFileSync(join(DATA_DIR, name), 'utf8')) as T
}

// --- tiers -----------------------------------------------------------------

export type BuildPath = {
  id: string
  name: string
  order: number
  available: boolean
  one_liner: string
}

export type Tier = {
  id: string
  name: string
  order: number
  tagline: string
  adds: string
  purpose: string
  good_for: string
  not_good_for: string
  capabilities: Record<string, boolean>
  difficulty: string
  build_time_estimate: string | null
  requires_note?: string
}

export type Tiers = {
  updated: string
  status: string
  upgrade_promise: string
  /** Why the promise is an intention rather than a fact. Rendered beside it. */
  upgrade_promise_verify?: string
  paths: BuildPath[]
  tiers: Tier[]
  scope: {
    does: string[]
    does_not: string[]
    boundary_statement: string
  }
  envelope: {
    target: string
    headroom: string
    regulatory_note: string
  }
}

export const getTiers = (): Tiers => load<Tiers>('tiers.yaml')

// --- bom -------------------------------------------------------------------

export type Applies = { tier: string; path: string; qty: number }

export type Substitute = {
  /** Build path on which this substitute is the part to buy, not the headline part. */
  recommended_for?: string
  mpn: string
  manufacturer?: string
  note: string
  url?: string
  confidence?: string
}

// A breakout entry can be an open question rather than a product: an
// acknowledgement that the Modules path needs an assembled board here and that
// nobody has found one yet. Those carry `verify` and nothing else, so
// everything identifying a specific product is optional.
export type Breakout = {
  confidence: string
  supplier?: string
  product?: string
  url?: string
  checked?: string
  note?: string
  verify?: string
}

export type Part = {
  id: string
  role: string
  designators?: string[]
  availability?: string
  breakout?: Breakout
  name: string
  manufacturer: string | null
  mpn: string | null
  confidence: 'asserted' | 'supplier' | 'unverified'
  price_usd: number | null
  mass_g: number | null
  verify?: string
  applies: Applies[]
  why: string
  optional?: boolean
  gotcha?: string
  region_note?: string
  substitutes: Substitute[]
}

export type Bom = {
  updated: string
  status: string
  currency: string
  paths: { id: string; name: string; summary: string; tradeoff: string; available: boolean }[]
  tier_budgets: {
    id: string
    target_bom_usd: number | null
    target_mass_g: number | null
  }[]
  parts: Part[]
  ground_station: {
    summary: string
    status: string
    /** The undecided question behind the whole approach. Renders on the page. */
    approval_note?: string
    parts: { id: string; name: string; qty: number; verify?: string }[]
  }
  tools: {
    required: { name: string; note?: string }[]
    recommended: { name: string; note?: string }[]
    board_path: { name: string; note?: string }[]
    fabrication: { name: string; note?: string }[]
  }
}

export const getBom = (): Bom => load<Bom>('bom.yaml')

// --- suppliers -------------------------------------------------------------

export type Supplier = { id: string; name: string; search: string; note?: string }

export type Suppliers = {
  updated: string
  distributors: Supplier[]
  makers: Supplier[]
}

export const getSuppliers = (): Suppliers => load<Suppliers>('suppliers.yaml')

/** Fill a supplier's search template with a manufacturer part number. */
export const supplierSearch = (s: Supplier, mpn: string) =>
  s.search.replace('{mpn}', encodeURIComponent(mpn))

// --- glossary --------------------------------------------------------------

export type Term = {
  id: string
  term: string
  aliases?: string[]
  short: string
  long: string
  see_also?: string[]
  source?: string
}

export type Glossary = { updated: string; status: string; terms: Term[] }

export const getGlossary = (): Glossary => load<Glossary>('glossary.yaml')

// --- flight phases ---------------------------------------------------------

export type Phase = {
  id: string
  name: string
  order: number
  summary: string
  behaviour: string
  caution?: string
  transitions_to: string | null
  criteria: string
  threshold: number | null
}

export type FlightPhases = { updated: string; status: string; phases: Phase[] }

export const getFlightPhases = (): FlightPhases => load<FlightPhases>('flight-phases.yaml')

// --- preflight -------------------------------------------------------------

export type PreflightItem = {
  id: string
  text: string
  why: string
  tiers: string[]
  critical?: boolean
}

export type PreflightSection = {
  id: string
  title: string
  where: string
  items: PreflightItem[]
}

export type Preflight = {
  updated: string
  status: string
  intro: string
  sections: PreflightSection[]
  footer: string
}

export const getPreflight = (): Preflight => load<Preflight>('preflight.yaml')

// --- troubleshooting -------------------------------------------------------

export type Check = { do: string; detail?: string; critical?: boolean }

export type TroubleEntry = {
  id: string
  category: string
  symptom: string
  tiers: string[]
  checks: Check[]
  note?: string
  see_also?: string[]
}

export type Troubleshooting = {
  updated: string
  status: string
  categories: { id: string; title: string }[]
  entries: TroubleEntry[]
}

export const getTroubleshooting = (): Troubleshooting => load<Troubleshooting>('troubleshooting.yaml')

// --- flight log ------------------------------------------------------------

export type Flights = {
  updated: string
  status: string
  submissions_open: boolean
  submissions_note: string
  submission: {
    required_files: { name: string; from: string; why?: string; optional?: boolean }[]
    derived_fields: { field: string; source: string }[]
    declared_fields: {
      field: string
      required: boolean
      example?: string
      why?: string
      options?: string[]
    }[]
    never_collected: { field: string; why: string }[]
  }
  archive_rules: { id: string; rule: string }[]
  flights: unknown[]
}

export const getFlights = (): Flights => load<Flights>('flights.yaml')

// --- mechanical -------------------------------------------------------------

export type MechProvenance = 'standard' | 'derived' | 'practice' | 'provisional'

export type MechParam = {
  id: string
  group: string
  value: number
  unit: string
  provenance: MechProvenance
  what: string
  /** Present on everything except provisional values. */
  source?: string
  /** Present only on provisional values: what evidence retires the guess. */
  closes?: string
}

export type MechPart = {
  id: string
  name: string
  form: string
  source: string
  note: string
  prints_with: string
}

export type Mechanical = {
  updated: string
  status: string
  groups: { id: string; name: string; note: string }[]
  params: MechParam[]
  parts: MechPart[]
}

export const getMechanical = () => load<Mechanical>('mechanical.yaml')

/**
 * The overall size of each printed part, measured off its exported STL by
 * tools/render-mechanical.mjs.
 *
 * Read from the render manifest rather than from mechanical.yaml because it is
 * an output of the geometry, not an input to it: the parts are built from two
 * dozen parameters through unions, differences and a hull, so a hand-typed
 * envelope would be a fourth place for the same number to drift. This one is
 * recomputed every time the models are rendered.
 */
export const getPartSizes = (): Record<string, { x_mm: number; y_mm: number; z_mm: number }> => {
  const manifest = parse(
    readFileSync(join(REPO_ROOT, 'hardware/mechanical/rendered.json'), 'utf8')
  ) as { sizes?: Record<string, { x_mm: number; y_mm: number; z_mm: number }> }
  return manifest.sizes ?? {}
}

// --- pcb status -------------------------------------------------------------

export type PcbStatus = {
  fab_ready: boolean
  components: number
  nets: number
  routed_traces: number
  vias: number
  blockers: { id: string; count: number; what: string }[]
  advisories: string[]
}

/**
 * Generated by tools/check-pcb.mjs, rendered rather than described.
 *
 * The schematic page used to carry a paragraph explaining why there was no PCB.
 * There is one now, and a hand-written paragraph about the state of a board is
 * exactly the thing that goes stale the day after somebody fixes something. The
 * checker writes this file and the build fails if it is out of date, so the page
 * cannot claim the board is closer to fabricable than it is.
 */
export const getPcbStatus = (): PcbStatus =>
  parse(readFileSync(join(REPO_ROOT, 'hardware/pcb-status.json'), 'utf8')) as PcbStatus
