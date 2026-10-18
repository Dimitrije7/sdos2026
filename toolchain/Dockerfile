# syntax=docker.io/docker/dockerfile:1.27.1@sha256:4edf897a3ffa55b89f906fc8cc78afdb3f1834cc9c7083565e611a8a7d5fe99e
# check=skip=InvalidDefaultArgInFrom
#
# SDOS development and CI containers. Build through `make bootstrap`, which
# passes every pin below from manifest/versions.json; nothing here names a
# version on its own except the frontend digest above, checked by ci/test_container.py.
#
# Integrity: apt verifies the snapshot's signed InRelease files and package
# hashes; this file additionally checks each InRelease and each requested .deb
# against the manifest SHA-256. TLS is therefore not needed for integrity, which
# lets the bare base image fetch ca-certificates before it can verify TLS peers.

ARG BASE_IMAGE
# oss-base, or ti-sdk once the user has accepted TI's terms.
ARG TOOLCHAIN_BASE=oss-base

FROM ${BASE_IMAGE} AS oss-base
ARG APT_SNAPSHOT
ARG APT_INRELEASE_SHA256
ARG APT_PACKAGES
ARG APT_DEB_SHA256
ENV DEBIAN_FRONTEND=noninteractive LANG=C.UTF-8 LC_ALL=C.UTF-8
# errexit makes every failed check stop the build.
SHELL ["/bin/bash", "-euo", "pipefail", "-c"]
# The cache mount keeps downloaded .deb files between local rebuilds; they are
# rechecked against the manifest every time and never enter the image.
# APT_PACKAGES pins every package as name=version, which hadolint cannot see.
# hadolint ignore=DL3008
RUN --mount=type=cache,target=/var/cache/apt/archives,sharing=locked \
    rm -f /etc/apt/apt.conf.d/docker-clean; \
    : "${APT_SNAPSHOT:?}" "${APT_INRELEASE_SHA256:?}" "${APT_PACKAGES:?}" "${APT_DEB_SHA256:?}"; \
    mirror="https://snapshot.ubuntu.com/ubuntu/${APT_SNAPSHOT}"; \
    rm -f /etc/apt/sources.list.d/*; \
    printf 'deb %s %s main universe\n' \
      "$mirror" jammy "$mirror" jammy-updates "$mirror" jammy-security > /etc/apt/sources.list; \
    printf 'Acquire::Check-Valid-Until "false";\nAPT::Install-Recommends "false";\n' \
      > /etc/apt/apt.conf.d/50sdos-snapshot; \
    apt-get -o Acquire::https::Verify-Peer=false update; \
    for pair in ${APT_INRELEASE_SHA256}; do \
      echo "${pair#*=}  /var/lib/apt/lists/snapshot.ubuntu.com_ubuntu_${APT_SNAPSHOT}_dists_${pair%%=*}_InRelease"; \
    done | sha256sum --check --strict; \
    apt-get -o Acquire::https::Verify-Peer=false install --no-install-recommends --download-only --yes ${APT_PACKAGES}; \
    for pair in ${APT_DEB_SHA256}; do \
      echo "${pair#*=}  /var/cache/apt/archives/${pair%%=*}"; \
    done | sha256sum --check --strict; \
    apt-get install --no-install-recommends --yes ${APT_PACKAGES}; \
    dpkg-query --show --showformat '${Package}=${Version}\n' | sort > /usr/share/sdos-packages.txt; \
    rm -rf /var/lib/apt/lists/*
# Python tools, every wheel checked against the lock file's SHA-256.
COPY python-tools.lock /usr/share/sdos-python-tools.lock
# Unversioned names are added for tools that Ubuntu installs with a version suffix.
RUN python3 -m pip install --no-cache-dir --require-hashes --no-deps --only-binary=:all: \
      -r /usr/share/sdos-python-tools.lock; \
    for tool in clang clang-format clang-tidy clangd; do \
      ln -sf "/usr/bin/${tool}-15" "/usr/local/bin/${tool}"; \
    done
# Single-file tools from their upstream releases, each checked against the
# manifest SHA-256 before it is unpacked. FILE_TOOLS holds "name|url|sha256".
ARG FILE_TOOLS
RUN : "${FILE_TOOLS:?}"; \
    for entry in ${FILE_TOOLS}; do \
      name=${entry%%|*}; rest=${entry#*|}; url=${rest%%|*}; sha=${rest#*|}; \
      curl --fail --silent --show-error --location --retry 3 --output "/tmp/$name.download" "$url"; \
      echo "$sha  /tmp/$name.download" | sha256sum --check --strict; \
      case "$name" in \
        hadolint) install -m 0755 /tmp/hadolint.download /usr/local/bin/hadolint ;; \
        syft) tar -xzf /tmp/syft.download -C /usr/local/bin syft ;; \
        trivy) tar -xzf /tmp/trivy.download -C /usr/local/bin trivy ;; \
        node) mkdir -p /opt/node && tar -xJf /tmp/node.download -C /opt/node --strip-components=1 ;; \
        actionlint) tar -xzf /tmp/actionlint.download -C /usr/local/bin actionlint ;; \
        *) echo "unknown tool $name" >&2; exit 1 ;; \
      esac; \
      rm -f "/tmp/$name.download"; \
    done
ENV PATH=/opt/node/bin:${PATH}
# Node.js tools; npm ci verifies every package against package-lock.json.
COPY node/package.json node/package-lock.json /opt/sdos-node/
RUN npm ci --prefix /opt/sdos-node --ignore-scripts --no-audit --no-fund \
    && ln -s /opt/sdos-node/node_modules/.bin/markdownlint-cli2 /usr/local/bin/markdownlint-cli2
ENV SDOS_IN_CONTAINER=1
WORKDIR /work

# TI C6000 and C7000 compilers. This layer and ti-sdk exist only in local
# images and trusted CI jobs: the license matrix prohibits publishing them, and
# the facade builds them only after `make bootstrap TI=accept`. TI_TOOLS holds
# "family|url|sha256"; each installer is checked before it runs.
FROM oss-base AS ti-toolchain
SHELL ["/bin/bash", "-euo", "pipefail", "-c"]
ARG TI_TOOLS
RUN : "${TI_TOOLS:?}"; \
    mkdir -p /opt/ti; \
    for entry in ${TI_TOOLS}; do \
      family=${entry%%|*}; rest=${entry#*|}; url=${rest%%|*}; sha=${rest#*|}; \
      curl --fail --silent --show-error --location --retry 3 --output "/tmp/$family.bin" "$url"; \
      echo "$sha  /tmp/$family.bin" | sha256sum --check --strict; \
      chmod +x "/tmp/$family.bin"; \
      if ! "/tmp/$family.bin" --mode unattended --prefix "/opt/ti/install-$family" > "/tmp/$family.log" 2>&1; then \
        tail -n 20 "/tmp/$family.log" >&2; exit 1; \
      fi; \
      digit=${family#c}; digit=${digit%000}; \
      compiler=$(find "/opt/ti/install-$family" -type f -perm -u+x -name "cl${digit}x" | head -n 1); \
      test -n "$compiler"; \
      ln -s "$(dirname "$(dirname "$compiler")")" "/opt/ti/$family"; \
      echo "$sha" > "/opt/ti/$family.installer.sha256"; \
      rm -f "/tmp/$family.bin" "/tmp/$family.log"; \
    done; \
    /opt/ti/c6000/bin/cl6x --compiler_revision; \
    /opt/ti/c7000/bin/cl7x --compiler_revision
ENV SDOS_TI_C6000_ROOT=/opt/ti/c6000 SDOS_TI_C7000_ROOT=/opt/ti/c7000

# The parts of TI's Processor SDK RTOS that course firmware needs: PDK with
# FreeRTOS and drivers, the C66x and C7x DSP libraries, and the SDK build
# scripts. `make bootstrap` downloads the archive once into the user's cache
# and passes that directory as the ti-inputs build context; the archive is
# checked against the manifest SHA-256 again before extraction.
FROM ti-toolchain AS ti-sdk
SHELL ["/bin/bash", "-euo", "pipefail", "-c"]
ARG PSDK_FILE
ARG PSDK_SHA256
ARG PSDK_COMPONENTS
RUN --mount=type=bind,from=ti-inputs,target=/mnt/ti-inputs \
    : "${PSDK_FILE:?}" "${PSDK_SHA256:?}" "${PSDK_COMPONENTS:?}"; \
    echo "${PSDK_SHA256}  /mnt/ti-inputs/${PSDK_FILE}" | sha256sum --check --strict; \
    mkdir -p /opt/ti/psdk; \
    root=${PSDK_FILE%.tar.gz}; \
    set --; for component in ${PSDK_COMPONENTS}; do set -- "$@" "$root/$component"; done; \
    tar -xzf "/mnt/ti-inputs/${PSDK_FILE}" -C /opt/ti/psdk "$@"; \
    ln -s "/opt/ti/psdk/$root" /opt/ti/psdk/current; \
    echo "${PSDK_SHA256}" > /opt/ti/psdk.archive.sha256
ENV SDOS_TI_PSDK_ROOT=/opt/ti/psdk/current

# Non-interactive builds and checks; CI and `make` commands run here.
# hadolint ignore=DL3006
FROM ${TOOLCHAIN_BASE} AS ci

# Interactive development: the same tools, used by .devcontainer.
# hadolint ignore=DL3006
FROM ${TOOLCHAIN_BASE} AS dev

# Documentation checks.
FROM oss-base AS docs
