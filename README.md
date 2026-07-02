# Styio Cloud

**Globally scalable cloud-computing service platform for the Styio ecosystem.**
Part of the [Styio](https://styio.io) ecosystem.

[![License](https://img.shields.io/github/license/SymPolicy/Styio-Cloud?style=flat-square)](LICENSE)

> Repository status: `SymPolicy/Styio-Cloud` was archived on 2026-04-30.
> Active downstream maintenance now lives at
> <https://github.com/Unka-Malloc/Styio-Cloud> on the `nightly` branch.

---

Styio Cloud owns hosted compile services, multi-region deployment nodes,
package repository distribution, mirror synchronization, registry control-plane
server surfaces, and extensible cloud-service validation tooling.

This repository is downstream of Styio and Pafio:

- **Styio** remains the compiler and language implementation source of truth.
- **Pafio** remains the package manager, resolver, pack, publish, and client
  workflow surface.
- **Styio Cloud** consumes those contracts to run hosted workspaces, compile
  jobs, registry control planes, deployment nodes, mirror sites, and cloud
  stress gates.

## Product Role

- Global hosted compile and execution platform
- Default hosted compile target for Styio workspaces
- Standard C++/LLVM development and compile environment for every workspace
- Native C++ invocation path for mixed Styio/C++ workloads
- Package repository distribution foundation for Pafio
- Multi-region and cross-network deployment node control plane
- Mirror-site synchronization and registry replication
- Extensible cloud service APIs consumed by local and hosted clients

## V1 Service Kernel

The first runnable cloud service kernel targets a pure C++ implementation:

| Component | Technology |
|---|---|
| HTTP service layer | Boost.Beast + Boost.Asio |
| Durable state | Postgres |
| Object storage | Provider-neutral; S3 as first backend |
| Internal security | mTLS between all service components |
| Initial deployment | Single-region before multi-region promotion |

The executable contract source lives under
`contracts/styio-cloud-control-plane/v1/`.

## Validate

```sh
cmake -S . -B build-codex -DSTYIO_CLOUD_BUILD_TESTS=ON
cmake --build build-codex
ctest --test-dir build-codex --output-on-failure
python3 -m unittest tests/unit/test_cloud_compile_stress.py
python3 scripts/docs-audit.py
python3 scripts/repo-hygiene-gate.py --mode tracked
```

## License

Apache-2.0. See [LICENSE](LICENSE).
