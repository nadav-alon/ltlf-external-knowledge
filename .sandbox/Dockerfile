# The image a manager run on this project executes in: the shared sandbox
# image plus the C++ toolchain, Spot and MONA.
#
# Spot is built from its release tarball, not installed from Spot's Debian
# repository: those packages need a newer libc than the shared image's Debian
# release carries. It is built in a stage of its own so that a rebuild of the
# shared image reuses the compiled Spot rather than compiling it again.
#
# SPOT_VERSION is the version on the developer's machine. The CI workflow
# (.github/workflows/ci.yml) pins the same one; bump both together, in a ticket
# of its own.
FROM debian:bookworm-slim AS spot-build

ARG SPOT_VERSION=2.15.1

RUN apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates \
      curl \
      g++ \
      make \
      python3 \
    && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL "https://www.lrde.epita.fr/dload/spot/spot-${SPOT_VERSION}.tar.gz" \
      | tar -xz -C /tmp \
    && cd "/tmp/spot-${SPOT_VERSION}" \
    && ./configure --prefix=/usr/local --disable-python --disable-devel --disable-static \
    && make -j"$(nproc)" \
    && make install-strip DESTDIR=/spot \
    && rm -rf "/tmp/spot-${SPOT_VERSION}"

FROM side-projects-sandbox:latest

USER root

RUN apt-get update && apt-get install -y --no-install-recommends \
      g++ \
      make \
      cmake \
      pkg-config \
      python3 \
      mona \
    && rm -rf /var/lib/apt/lists/*

COPY --from=spot-build /spot/usr/local/ /usr/local/
RUN ldconfig

USER node
