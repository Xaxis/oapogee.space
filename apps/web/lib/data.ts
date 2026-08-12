import { readFileSync } from 'node:fs'
import { join } from 'node:path'
import { parse } from 'yaml'
import { DATA_DIR } from './repo'

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
  mpn: string
  manufacturer?: string
  note: string
  confidence?: string
}

export type Part = {
  id: string
  role: string
  name: string
  manufacturer: string | null
  mpn: string | null
  confidence: 'asserted' | 'unverified'
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
    parts: { id: string; name: string; qty: number }[]
  }
  tools: {
    required: { name: string; note?: string }[]
    recommended: { name: string; note?: string }[]
    fabrication: { name: string; note?: string }[]
  }
}

export const getBom = (): Bom => load<Bom>('bom.yaml')

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
