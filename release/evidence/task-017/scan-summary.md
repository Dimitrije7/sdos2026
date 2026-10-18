# Task 017 container scan, 2026-10-18

Image `sdos-dev:dev-abd36b351643`, ID
`sha256:8b4df442a5dd276d273a110df0a85a5f31df9bbb9604f627f51b2e1ae90a8037`,
671 MB, built from commit `92b38d3` plus the Task 017 lint changes.

## SBOM

`sbom.spdx.json.xz` is the Syft 1.54.0 SPDX inventory of the image
filesystem, excluding `/proc`, `/sys`, and the mounted repository: 1,016
packages. SHA-256 of the compressed file:
`845f95f5eb6ea7c494840b3ce9c2a0fc138f9aea39207149188ae8f8c505f889`.

## Vulnerability scan

Trivy 0.75.0, `trivy rootfs --scanners vuln`, database updated
2026-10-18T13:11:41Z (see `trivy-version.json`). The full JSON is not archived
because the database is a mutable input; rerun the command to reproduce.

| Severity | Findings |
|---|---|
| Critical | 7 |
| High | 329 |
| Medium | 2,215 |
| Low | 852 |
| Unknown | 2 |

- All 7 critical and 315 of the high findings are kernel CVEs reported against
  `linux-libc-dev` 5.15.0-198.208, the kernel header package used for
  compiling. No kernel runs in the container, and Ubuntu lists no fixed
  version.
- `libssl3` 3.0.2-0ubuntu1.29 has one high finding fixed in
  3.0.2-0ubuntu1.30, published after the pinned snapshot of 2026-10-04. Moving
  the snapshot is a manifest revision.
- The other high findings are in the npm copy bundled with Node.js (5 in
  `brace-expansion`, and single findings in `pacote`, `braces`,
  `http-cache-semantics`, `ip-address`, `picomatch`, and `sigstore`) and in
  the Go standard library and `golang.org/x/net` compiled into the Syft and
  Trivy binaries.

Task 024 owns the severity policy, the database update and retention rule,
and the decision on when a finding requires a manifest revision.
