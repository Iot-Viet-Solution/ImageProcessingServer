# API Reference

Base URL (default): `http://localhost:8080`

All processing is **stateless**: images are read from the request, transformed
in memory, and returned in the response. Nothing is written to disk, logged as
pixels, or cached server-side (`Cache-Control: no-store`).

---

## `POST /v1/process`

Decode an image, apply a transform pipeline, and return the re-encoded result.

### Request

**Body** — supply the source image one of two ways:

| Mode                    | How                                                      |
|-------------------------|---------------------------------------------------------|
| Raw bytes               | Put the image bytes directly in the body (`--data-binary`) |
| `multipart/form-data`   | One file part named `image` (or `file`; a lone part is also accepted) |

**Operations** — supplied as URL **query parameters**. All are optional; with no
parameters the image is decoded and re-encoded in its source format.

| Param       | Type / values                            | Default        | Description                                                            |
|-------------|------------------------------------------|----------------|------------------------------------------------------------------------|
| `w`         | integer > 0                              | —              | Target width in pixels.                                                |
| `h`         | integer > 0                              | —              | Target height in pixels.                                               |
| `fit`       | `inside` \| `outside` \| `cover` \| `fill` | `inside`     | How the image is fitted into the `w`×`h` box (see [Fit modes](#fit-modes)). |
| `crop`      | `x,y,w,h` (integers)                     | —              | Crop rectangle in source pixels, applied **before** resize.            |
| `rotate`    | integer (degrees, clockwise)             | —              | `90`/`180`/`270` are lossless re-orientation; other angles do an affine rotate. |
| `flip`      | `h` \| `v` \| `hv`                       | —              | Mirror horizontally, vertically, or both.                              |
| `grayscale` | `1` \| `true` \| `yes`                   | `false`        | Convert to single-channel grayscale.                                   |
| `blur`      | float ≥ 0                                | —              | Gaussian blur sigma (larger = blurrier).                               |
| `format`    | `jpeg` \| `png` \| `webp` \| `avif` \| `gif` \| `tiff` | source format | Output encoding.                                          |
| `q`         | integer 1–100                            | encoder default | Output quality for lossy formats (`jpeg`, `webp`, `avif`). Ignored for lossless. |

#### Pipeline order

Operations always apply in this fixed order, regardless of query-string order:

```
crop → resize (w/h/fit) → rotate → flip → grayscale → blur → encode (format/q)
```

> When `w`/`h` are set **without** `crop`, resizing uses libvips
> *shrink-on-load* (decode directly at the reduced size) for speed and low
> memory. With `crop`, the image is decoded, cropped, then resized.

### Response

| On      | Status | Body                                  | Notable headers                          |
|---------|--------|---------------------------------------|------------------------------------------|
| Success | `200`  | Encoded image bytes                   | `Content-Type: image/<fmt>`, `Cache-Control: no-store` |
| Bad params / no image | `400` | `{"error": "..."}`        | `application/json`                        |
| Undecodable / invalid op | `422` | `{"error": "..."}`     | `application/json`                        |
| Queue full (backpressure) | `503` | `{"error": "server busy..."}` | `Retry-After: 1`                    |
| Internal error | `500` | `{"error": "..."}`             | `application/json`                        |

### Examples

```bash
# Resize to 300px wide (aspect preserved), output WebP at quality 80
curl -X POST --data-binary @photo.jpg \
  "http://localhost:8080/v1/process?w=300&format=webp&q=80" -o out.webp

# 200x200 square thumbnail, fill + centre-crop
curl -X POST --data-binary @photo.jpg \
  "http://localhost:8080/v1/process?w=200&h=200&fit=cover" -o thumb.jpg

# Crop a 400x300 region at (50,20), then convert to PNG
curl -X POST --data-binary @photo.jpg \
  "http://localhost:8080/v1/process?crop=50,20,400,300&format=png" -o crop.png

# Multipart upload: rotate 90°, grayscale, AVIF out
curl -X POST -F "image=@photo.jpg" \
  "http://localhost:8080/v1/process?rotate=90&grayscale=1&format=avif&q=50" -o out.avif

# Light blur, keep source format
curl -X POST --data-binary @photo.png \
  "http://localhost:8080/v1/process?blur=3" -o blurred.png
```

### Fit modes

Given a target box `w`×`h`:

| Mode      | Aspect ratio | Result                                                            |
|-----------|--------------|-------------------------------------------------------------------|
| `inside`  | preserved    | Largest size that fits **within** the box (no side exceeds it).   |
| `outside` | preserved    | Smallest size that **covers** the box (one side may exceed it).   |
| `cover`   | preserved    | Fills the box exactly, **centre-cropping** the overflow.          |
| `fill`    | ignored      | Stretched to **exactly** `w`×`h`.                                 |

> Supplying only `w` **or** only `h` resizes by that single dimension with the
> aspect ratio preserved (the other axis is unconstrained).

---

## `GET /healthz`

Liveness probe. Returns `200` with `{"status":"ok"}` whenever the process is up.
Does not touch the worker pool.

```bash
curl http://localhost:8080/healthz
# {"status":"ok"}
```

---

## `GET /readyz`

Readiness probe with worker-pool telemetry. Returns `503` when the queue is
saturated (the server is shedding load), `200` otherwise.

```json
{
  "status": "ready",          // or "saturated"
  "workers": 8,               // image worker threads
  "queue_pending": 0,         // jobs queued, not yet started
  "queue_capacity": 256       // max_queue before 503s begin
}
```

---

## `GET /v1/version`

Build and engine info.

```json
{
  "name": "ImageProcessingServer",
  "version": "0.1.0",
  "engine": "libvips",
  "libvips": "8.16.0"
}
```

---

## CORS

`POST /v1/process` answers `OPTIONS` preflight and sets
`Access-Control-Allow-Origin: *` on responses, so it can be called directly
from browser front-ends. Tighten this in `ProcessController` if you need to
restrict origins.

## Limits & tuning

These come from `config/config.json`:

| Setting                          | Controls                                              |
|----------------------------------|-------------------------------------------------------|
| `app.client_max_body_size`       | Max upload size (bytes). Larger uploads are rejected.  |
| `custom_config.max_image_pixels` | Max decoded `width*height` (decompression-bomb guard). |
| `custom_config.max_queue`        | Queue depth before `503` backpressure kicks in.        |
| `custom_config.workers`          | Image worker threads (`0` = CPU core count).           |

See the [main README](../README.md#configuration) for the full configuration table.
