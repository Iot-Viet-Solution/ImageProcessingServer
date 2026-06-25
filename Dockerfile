# syntax=docker/dockerfile:1

# ---- Build stage ---------------------------------------------------------
# The official Drogon image already ships Drogon + Trantor (built static) and
# its build toolchain, so we only add libvips and compile the app.
FROM drogonframework/drogon:latest AS build

RUN apt-get update \
    && apt-get install -y --no-install-recommends libvips-dev pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
# Only what's needed to compile — config is copied into the runtime stage, so
# editing config does not invalidate the (expensive) compile cache layer.
COPY CMakeLists.txt ./
COPY src/ ./src/

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --target image_server -j "$(nproc)"

# ---- Runtime stage -------------------------------------------------------
# Match the build stage's distro (the Drogon base image is Ubuntu 22.04 / jammy)
# so the libvips and system-library ABIs line up with the compiled binary.
FROM ubuntu:22.04 AS runtime

# Runtime shared libraries: libvips + the system deps Drogon links against.
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        libvips42 \
        libjsoncpp25 \
        libssl3 \
        libuuid1 \
        zlib1g \
        libc-ares2 \
        libbrotli1 \
        ca-certificates \
        curl \
        # Drogon in the base image is built with DB client support, so the
        # binary links these even though this service uses no database.
        libpq5 \
        libmariadb3 \
        libhiredis0.14 \
    && rm -rf /var/lib/apt/lists/*

# Run as an unprivileged user.
RUN useradd --system --uid 10001 --no-create-home appuser

WORKDIR /app
COPY --from=build /src/build/image_server /app/image_server
# Config comes from the build context (not the build stage), so editing it
# rebuilds only this cheap layer rather than recompiling.
COPY config/ /app/config

USER appuser
EXPOSE 8080

HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD curl -fsS http://localhost:8080/healthz || exit 1

ENTRYPOINT ["/app/image_server", "/app/config/config.json"]
