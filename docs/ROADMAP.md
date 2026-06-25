# Roadmap

Direction for ImageProcessingServer. Items are grouped by target milestone, not
hard-committed dates. Each entry notes *why* and *where* it plugs into the
current architecture (Drogon controllers → bounded `JobQueue` → `ImageProcessor`
on libvips, fully stateless).

**Guiding constraints** (unchanged across all milestones):

- **Stateless, no data storage.** Nothing is persisted between requests.
- **MIT-clean licensing.** Only permissive or weak-copyleft (LGPL, *dynamically
  linked*) dependencies — no GPL-static, no commercial components.
- **Backpressure over latency.** Bounded queue sheds load (`503`) rather than
  letting work pile up.

Legend: `[ ]` planned · `[~]` in progress · `[x]` shipped

---

## v0.1 — Shipped ✅

The image-processing core. Verified end-to-end in Docker.

- [x] `POST /v1/process` — resize, fit modes (`inside`/`outside`/`cover`/`fill`),
      crop, rotate (90/180/270 + arbitrary), flip, grayscale, blur
- [x] Format conversion + quality: JPEG, PNG, WebP, **AVIF**, GIF, TIFF
- [x] Raw-body and `multipart/form-data` uploads
- [x] Bounded worker-pool queue with `503 Retry-After` backpressure
- [x] Decompression-bomb guard (`max_image_pixels`)
- [x] `/healthz`, `/readyz` (pool telemetry), `/v1/version`
- [x] Multi-stage Docker image (non-root, healthcheck), Compose, CI

---

## v0.2 — Hardening & observability ✅ Shipped

Make it safe and operable to expose.

- [x] **API-key / bearer-token auth** — `ApiKeyFilter` gating `/v1/process`,
      keys from config. Off by default; `X-API-Key` or `Authorization: Bearer`.
- [x] **Per-client rate limiting** — token bucket keyed by API key / IP, limits
      in `custom_config`. Over-limit → `429 Retry-After`.
- [x] **Prometheus `/metrics`** — request count, latency histogram, process
      results (ok/error/rejected/timeout), output bytes, live queue gauges.
- [x] **Structured request logging** — method, path, status, bytes, duration,
      client IP. No pixel data. Toggle via `observability.access_log`.
- [x] **Per-request timeout** — deadline (queue wait + compute) aborts long jobs
      via the libvips kill watchdog → `504`.
- [x] **Graceful-shutdown drain** — SIGTERM/SIGINT flips `/readyz` to `503`,
      drains in-flight jobs, then stops the loop so responses still deliver.
- [x] **Configurable CORS** — origin allowlist from config, applied globally.

## v0.3 — Richer image operations

Broaden the transform pipeline in `ImageProcessor`. Each maps to a libvips call,
so cost is low; the work is API design + validation in `ProcessOptions`.

- [ ] **Sharpen** (`sharpen=`) — `VImage::sharpen` after resize.
- [ ] **Brightness / contrast / gamma** — `linear` + `gamma`.
- [ ] **Saturation / tint / grayscale-with-colour** — colourspace ops.
- [ ] **Flatten against background** (`bg=`) — composite transparency onto a
      colour; also fixes arbitrary-angle rotate corners.
- [ ] **EXIF auto-orient** — honour orientation on load (thumbnail already does;
      extend to the non-resize path).
- [ ] **Smart crop** for `fit=cover` — `VIPS_INTERESTING_ATTENTION/ENTROPY`
      instead of centre, opt-in via a param.
- [ ] **Watermark / text / composite overlay** — overlay a second image or text;
      needs a small multi-input request shape.

## v0.4 — Delivery & integration

Make it pleasant to put behind a CDN / call from many services.

- [ ] **Signed URLs (HMAC)** — imgproxy-style signed request paths so the
      endpoint can be exposed publicly without an open transform proxy.
- [ ] **Named presets** — server-defined transform profiles (e.g. `preset=thumb`)
      in config, so clients send a name instead of a long query string.
- [ ] **Content negotiation** — pick output format from the `Accept` header when
      `format` is omitted (serve AVIF/WebP to browsers that advertise them).
- [ ] **OpenAPI / Swagger spec** — generated `openapi.yaml` + served docs.
- [ ] **GET form for cacheable transforms** — `GET /v1/process?...` with the
      source by signed reference, so CDNs can cache by URL. (Still no
      server-side storage; the CDN owns the cache.)

## v1.0 — Production readiness

- [ ] **Benchmarks** — throughput/latency vs imgproxy/sharp, published numbers
      and a `bench/` harness.
- [ ] **Multi-arch images** — `linux/amd64` + `linux/arm64` published to GHCR.
- [ ] **Kubernetes manifests / Helm chart** — HPA on queue depth, resource
      requests/limits, readiness wired to `/readyz`.
- [ ] **Newer libvips option** — optional build path on a base with libvips ≥ 8.15
      for the latest codec/perf improvements (current image is 8.12, AVIF works).
- [ ] **Optional async job mode** — `POST` returns a job id, `GET` polls; for
      very large/slow batch jobs. Results held in-memory with a TTL, never on
      disk — preserves the stateless guarantee. Strictly opt-in.
- [ ] **Hardening pass** — fuzz the decoder path, tighten input caps, security
      review of the upload handling.

---

## Explicit non-goals

These are intentionally out of scope (see [FEATURES.md](FEATURES.md#non-goals-by-design)):

- Persistent storage / object store / result cache — use a CDN in front.
- HEIC *encode* — requires GPL HEVC encoder; conflicts with MIT-clean licensing.
- A built-in user/account system — front with a gateway.

## Contributing

Picking up a roadmap item? Keep changes within the existing layering
(controller → `JobQueue` → `ImageProcessor`), preserve statelessness, and verify
with `scripts/smoke-test.sh` against a running container before opening a PR.
