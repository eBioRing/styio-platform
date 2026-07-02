# styio-cloud Source

This source tree contains the first migrated platform kernel. It keeps imported
`pafio::` namespaces and file names stable while the repo split settles.

- `PafioPlan/` renders compile-plan v1 payloads.
- `PafioCloud/` renders cloud execution policy and build job requests.
- `PafioCore/`, `PafioManifest/`, `PafioResolve/`, `PafioRegistryClient/`,
  `PafioSecurity/`, and `PafioToolchain/` are the package-manager client
  dependencies needed to validate the platform payloads.
- `styio_cloud_stress/` owns the deterministic cloud compile stress harness.

New service implementation should prefer platform-owned names and APIs; the
imported names here are compatibility scaffolding.
