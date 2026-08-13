# oApogee: one entry point for everything.
#
# The premise of this repository is that the documentation is the product, which
# only means anything if the documentation's claims about itself are checkable.
# Every target here is also run by CI, so what a contributor runs locally and
# what the build runs cannot drift apart.
#
#   make          list targets
#   make check    everything CI runs
#   make dev      the site, locally

SHELL := /bin/bash
.DEFAULT_GOAL := help

.PHONY: help install dev build start lint type-check format test \
        check check-fast prose data links schematic check-schematic responsive \
        fw-build fw-test fw-check crossimpl check-suppliers mech clean

help: ## List available targets
	@grep -hE '^[a-z-]+:.*?## ' $(MAKEFILE_LIST) \
	  | awk 'BEGIN {FS = ":.*?## "}; {printf "  \033[1m%-16s\033[0m %s\n", $$1, $$2}'

install: ## Install dependencies, pinned to match CI
	yarn install

# --- verification ------------------------------------------------------------
# Each of these is a claim the repository makes about itself.

prose: ## Style rules hold, and the project name is never shortened to "Apogee"
	@node tools/check-prose.mjs

data: ## Structured data cross-references resolve and the accuracy contract holds
	@node tools/check-data.mjs
	@node tools/gen-mechanical.mjs --check

mech: ## Regenerate the printed parts. Needs OpenSCAD; CI does not have it.
	@node tools/gen-mechanical.mjs
	@node tools/render-mechanical.mjs

links: ## Every internal link resolves, and every anchor exists on its target
	@node tools/check-links.mjs

fw-build: ## Build the firmware core and its tests for the host
	@cmake -S firmware -B firmware/build -DCMAKE_BUILD_TYPE=Debug >/dev/null
	@cmake --build firmware/build

fw-test: fw-build ## Run the firmware test suite
	@ctest --test-dir firmware/build --output-on-failure

check-suppliers: ## Every supplier link still resolves. Not in `make check`: it hits the internet.
	@node tools/check-suppliers.mjs

fw-check: ## The passive boundary holds and no tuning constant is hardcoded
	@node tools/check-firmware.mjs

crossimpl: fw-build ## The firmware and the browser encode identical packets
	@node tools/check-crossimpl.mjs

test: ## Run the web test suite
	@yarn workspace @oapogee/web test

hw-deps: ## Install the tscircuit toolchain, isolated from the web workspace on purpose
	# npm ci rather than install, so the lockfile governs and a CI run cannot
	# resolve a different tree than a local one. Not silenced: this failed once
	# in CI and --silent meant the log said only that it had failed.
	cd hardware && npm ci --no-audit --no-fund

hw: ## Render the circuit source to schematic, netlist, circuit JSON and KiCad
	@node tools/build-hardware.mjs

check-hw: ## The committed hardware artifacts match hardware/oapogee.tsx
	@node tools/build-hardware.mjs --check

schematic: ## Regenerate the system block diagram from data/system.yaml
	@node tools/gen-schematic.mjs

check-schematic: ## The committed schematic matches the data it was drawn from
	@node tools/gen-schematic.mjs --check

check-fast: prose data links check-schematic fw-check lint type-check test ## Everything except the site build

check: check-fast build responsive fw-test crossimpl ## Everything CI runs

responsive: ## No page scrolls sideways at 320px or 390px. Needs a build and Chrome.
	@node tools/check-responsive.mjs

# --- the site ----------------------------------------------------------------

dev: ## Run the site locally
	yarn workspace @oapogee/web dev

build: ## Production build
	yarn workspace @oapogee/web build

start: ## Serve the production build
	yarn workspace @oapogee/web start

lint: ## ESLint the web workspace
	yarn workspace @oapogee/web lint

type-check: ## TypeScript, no emit
	yarn workspace @oapogee/web type-check

format: ## Prettier in place
	yarn prettier --write .

clean: ## Remove build output
	rm -rf apps/web/.next apps/web/.next-dev apps/web/out
