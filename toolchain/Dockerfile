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
    apt-get -o Acquire::https::Verify-Peer=false install --download-only --yes ${APT_PACKAGES}; \
    for pair in ${APT_DEB_SHA256}; do \
      echo "${pair#*=}  /var/cache/apt/archives/${pair%%=*}"; \
    done | sha256sum --check --strict; \
    apt-get install --yes ${APT_PACKAGES}; \
    dpkg-query --show --showformat '${Package}=${Version}\n' | sort > /usr/share/sdos-packages.txt; \
    rm -rf /var/lib/apt/lists/*
# Python tools, every wheel checked against the lock file's SHA-256.
COPY python-tools.lock /usr/share/sdos-python-tools.lock
RUN python3 -m pip install --no-cache-dir --require-hashes --no-deps --only-binary=:all: \
      -r /usr/share/sdos-python-tools.lock
# Unversioned names for tools that Ubuntu installs with a version suffix.
RUN for tool in clang clang-format clang-tidy clangd; do \
      ln -sf "/usr/bin/${tool}-15" "/usr/local/bin/${tool}"; \
    done
ENV SDOS_IN_CONTAINER=1
WORKDIR /work

# Non-interactive builds and checks; CI and `make` commands run here.
FROM oss-base AS ci

# Interactive development: the same tools, used by .devcontainer.
FROM oss-base AS dev
