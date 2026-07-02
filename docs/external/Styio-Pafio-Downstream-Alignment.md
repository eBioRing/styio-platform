# Styio And Pafio Downstream Alignment

**Purpose:** Record how `styio-cloud` stays downstream of both `styio` and `pafio` after the server migration.

**Last updated:** 2026-04-24

## Alignment Rules

- Compiler semantics and compile-plan execution capability come from `styio`.
- Package manifests, resolver behavior, lockfiles, pack, publish, and local CLI UX come from `pafio`.
- Hosted workspaces, cloud execution policy, registry control planes, and server-side service runbooks live in `styio-cloud`.
- Contract changes that affect a client must update the upstream client docs before platform docs claim closure.
