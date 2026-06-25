# Features

A summary of what ImageProcessingServer supports today, plus the design
guarantees and a forward-looking roadmap.

## Capabilities at a glance

| Area                | Status | Notes                                                        |
|---------------------|:------:|--------------------------------------------------------------|
| Resize              |   ✅   | By width, height, or both; shrink-on-load for speed.         |
| Fit modes           |   ✅   | `inside`, `outside`, `cover` (centre-crop), `fill` (stretch).|
| Crop                |   ✅   | Arbitrary `x,y,w,h` rectangle, applied before resize.        |
| Rotate              |   ✅   | Lossless 90/180/270; affine rotate for arbitrary angles.     |
| Flip / mirror       |   ✅   | Horizontal, vertical, or both.                               |
| Grayscale           |   ✅   | Single-channel `B_W` conversion.                             |
| Gaussian blur       |   ✅   | Configurable sigma.                                          |
| Format conversion   |   ✅   | Between all supported codecs (see below).                    |
| Quality control     |   ✅   | `q=1..100` for lossy encoders.                               |
| Backpressure queue  |   ✅   | Bounded worker pool; `503 Retry-After` when saturated.       |
| Decompression-bomb guard | ✅ | Configurable decoded-pixel cap.                            |
| Per-request timeout |   ✅   | Deadline (queue + compute) aborts long jobs → `504`.         |
| API-key auth        |   ✅   | `X-API-Key` / `Bearer`; off when no keys configured.         |
| Rate limiting       |   ✅   | Per-client token bucket (by key or IP) → `429`.              |
| Prometheus metrics  |   ✅   | `/metrics`: requests, latency, results, queue gauges.        |
| Access logging      |   ✅   | Structured per-request line; toggleable.                     |
| Configurable CORS   |   ✅   | Origin allowlist (`["*"]` = any).                            |
| Graceful shutdown   |   ✅   | SIGTERM drains in-flight jobs before exit; `/readyz` flips.  |
| Health / readiness  |   ✅   | `/healthz`, `/readyz` (with pool telemetry).                 |
| Docker / Compose    |   ✅   | Multi-stage build, non-root runtime, healthcheck.            |
| Stateless / no storage | ✅  | In-memory only; `Cache-Control: no-store`.                   |

## Supported image formats

Format availability depends on the libvips build (which optional loaders/savers
were compiled in). The table reflects a standard `libvips-dev` install.

| Format | Decode (input) | Encode (output) | Lossy (`q`) | Output `format=` value |
|--------|:--------------:|:---------------:|:-----------:|------------------------|
| JPEG   |       ✅       |       ✅        |     ✅      | `jpeg` (or `jpg`)      |
| PNG    |       ✅       |       ✅        |     —       | `png`                  |
| WebP   |       ✅       |       ✅        |     ✅      | `webp`                 |
| AVIF   |       ✅¹      |       ✅¹       |     ✅      | `avif`                 |
| GIF    |       ✅       |       ✅        |     —       | `gif`                  |
| TIFF   |       ✅       |       ✅        |     —       | `tiff` (or `tif`)      |
| HEIC/HEIF |    ✅¹      |       —²        |     —       | (decode only; re-encode as another format) |

¹ AVIF/HEIF support comes from libvips' `libheif` backend. **The provided
Docker image includes it** — AVIF encode and decode are verified working out of
the box (`libheif` is a hard dependency of the `libvips42` package). On a custom
build, confirm your libvips links `libheif`.
² HEIC *input* is decoded; HEIC *output* needs an HEVC encoder and is not
enabled — choose `avif`/`jpeg`/`webp` for output.

> When `format` is omitted, the server detects the source format and re-encodes
> in the same one (PNG fallback for anything otherwise unhandled).

## Operations reference

| Operation  | Parameter(s)             | Engine call (libvips)                  |
|------------|--------------------------|----------------------------------------|
| Resize     | `w`, `h`, `fit`          | `thumbnail` / `resize`                 |
| Crop       | `crop=x,y,w,h`           | `extract_area`                         |
| Rotate     | `rotate=deg`             | `rot` (90/180/270) or `rotate` (affine)|
| Flip       | `flip=h\|v\|hv`          | `flip`                                 |
| Grayscale  | `grayscale=1`            | `colourspace(B_W)`                     |
| Blur       | `blur=sigma`             | `gaussblur`                            |
| Encode     | `format`, `q`            | `write_to_buffer`                      |

See [API.md](API.md) for full parameter semantics and the fixed pipeline order.

## Design guarantees

- **Stateless** — no database, no disk writes, no result cache. Each request is
  independent; the service scales horizontally behind any load balancer.
- **Bounded resource use** — a fixed worker pool does the CPU work; a bounded
  queue provides backpressure instead of unbounded latency growth. libvips runs
  with `vips_concurrency=1` per operation so the pool owns parallelism (no CPU
  oversubscription).
- **Memory-efficient** — libvips streams and uses shrink-on-load, keeping peak
  memory low even for large images.
- **Safe inputs** — uploads are size-capped (`client_max_body_size`) and decoded
  pixels are capped (`max_image_pixels`) to resist decompression bombs.

## Non-goals (by design)

- **No persistence / no object storage.** The server never stores inputs or
  outputs. Put a CDN or cache in front if you need caching.
- **No async job/poll API.** Processing is synchronous request→response. (An
  async mode could be added — see roadmap.)
- **Optional built-in auth.** API-key auth ships in v0.2 (off by default); for
  richer identity, front it with a gateway/reverse proxy.

## Roadmap

Shipped in **v0.2** (see [ROADMAP.md](ROADMAP.md) for the full plan):

- [x] API-key / bearer-token auth filter
- [x] Per-client rate limiting
- [x] Prometheus `/metrics` endpoint
- [x] Structured access logging, per-request timeout, graceful shutdown, CORS

Candidates for future milestones:

- [ ] Sharpen, brightness/contrast/gamma, colour-tint adjustments
- [ ] Watermark / text overlay / composite operations
- [ ] Smart crop (attention/entropy-based) for `cover`
- [ ] Signed URL / HMAC request validation (imgproxy-style)
- [ ] Optional async job submission + polling for very large batch jobs
- [ ] Content-negotiation (`Accept:` header → best output format)

Licensing note: all current and proposed dependencies are permissive or
weak-copyleft (libvips is LGPL, **dynamically linked**), keeping the project
MIT-clean with no commercial components.
