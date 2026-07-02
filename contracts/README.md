# styio-cloud Contracts

This directory owns service-side and platform-side contract packages migrated
from `pafio`.

- `compile-plan/` is the compiler handoff format consumed by platform workers.
- `hosted-control-plane/` is the native JSON hosted workspace contract package.
- `styio-cloud-control-plane/` is the native JSON service-kernel contract package
  for the first runnable cloud control plane.
- `registry-control-plane/` is the native JSON server-side registry
  write/control contract package.
- `registry-v2/` is retained here for server validation and compatibility with
  `pafio` package-manager clients.

Contract packages are maintained as repo-native JSON contracts and examples.
Markdown describes ownership and stability rules; executable gates validate the
JSON packages directly.

`styio-cloud` owns the hosted registry and mirror service side. `pafio`
owns the local package-manager client, offline cache behavior, and import/export
semantics that consume these packages.
