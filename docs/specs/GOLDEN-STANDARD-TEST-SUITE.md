# Golden Standard Test Suite

**Purpose:** Define the styio-cloud test level that makes a service version submittable.

**Last updated:** 2026-04-26

`test / smoke` runs the fast Python unit suite under `tests/unit`.

`test / golden-standard` runs the platform delivery floor: documentation checks, public interop contract gates, native CMake tests, and Python service-tooling tests.

## Local Gate Profile

`styio-cloud-delivery-gate-profile` is the repository-owned adaptation for platform service delivery. It is maintained in this repository through `delivery-gate.sh`, public interop checks, native CMake checks, and service tooling evidence. The organization-level audit only verifies that this local profile is present and covered by `test / golden-standard`.

Required local markers: repo-owned adaptation, delivery-gate.sh, public interop, native CMake, service tooling.

## Industry Gate Group

`backend / service-security` is the role-specific gate group for styio-cloud service behavior. It keeps service security, public interop, dependency risk, runtime-secret, and deployment checks grouped under `test / golden-standard`.

Required evidence markers: auth boundary, deployment security, dependency vulnerability, runtime secret, interop contract, state-machine regression.

## Submit Readiness

A styio-cloud version is submittable only when `platform-adaptation / linux-ci-gate`, `test / smoke`, and `test / golden-standard` all pass.
