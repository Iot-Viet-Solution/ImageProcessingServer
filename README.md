# ImageProcessingServer

A high-performance, **stateless** C++ image-processing server. It exposes a REST
API to resize, crop, rotate, flip, blur, recolour, and convert images on the
fly. Images are processed entirely in memory and **never stored**.

- **Engine:** [libvips](https://www.libvips.org/) — streaming, low-memory,
  multi-threaded image processing (the same engine behind `sharp` and
  `imgproxy`).
- **HTTP:** [Drogon](https://github.com/drogonframework/drogon) — an
  asynchronous, top-tier-throughput C++17 web framework.
- **Backpressure:** a bounded worker queue. CPU-bound work runs off the I/O
  event loops; when the queue is full the server sheds load with `503`.
- **License:** MIT. All dependencies are permissively or weak-copyleft
  licensed; libvips (LGPL-2.1+) is **dynamically linked**, so this project
  stays MIT-clean. No commercially licensed components.

## API

### `POST /v1/process`

Send the source image as the raw request body **or** as a `multipart/form-data`
upload with an `image` (or `file`) field. Transform operations are passed as
query parameters. The processed image is returned in the response body.

| Param       | Values                                   | Description                                   |
|-------------|------------------------------------------|-----------------------------------------------|
| `w`         | int > 0                                  | Target width (px)                             |
| `h`         | int > 0                                  | Target height (px)                            |
| `fit`       | `inside` `outside` `cover` `fill`        | How to fit into `w`×`h` (default `inside`)    |
| `crop`      | `x,y,w,h`                                | Crop rectangle, applied before resize         |
| `rotate`    | degrees (clockwise)                      | 90/180/270 are lossless; other angles rotate  |
| `flip`      | `h` `v` `hv`                             | Mirror horizontally / vertically              |
| `grayscale` | `1` `true`                               | Convert to grayscale                          |
| `blur`      | float ≥ 0                                | Gaussian blur sigma                           |
| `format`    | `jpeg` `png` `webp` `avif` `gif` `tiff`  | Output format (default: keep source format)   |
| `q`         | 1–100                                    | Output quality for lossy formats              |

**Fit modes:** `inside` fits within the box; `outside` covers the box;
`cover` fills the box and centre-crops the overflow; `fill` stretches to the
exact box ignoring aspect ratio.

#### Examples

```bash
# Resize to 300px wide (aspect preserved), convert to WebP at q=80
curl -X POST --data-binary @photo.jpg \
  "http://localhost:8080/v1/process?w=300&format=webp&q=80" \
  -o out.webp

# Square 200x200 cover thumbnail
curl -X POST --data-binary @photo.jpg \
  "http://localhost:8080/v1/process?w=200&h=200&fit=cover" \
  -o thumb.jpg

# Multipart upload, rotate 90°, grayscale, PNG out
curl -X POST -F "image=@photo.jpg" \
  "http://localhost:8080/v1/process?rotate=90&grayscale=1&format=png" \
  -o out.png
```

### Operational endpoints

| Endpoint       | Purpose                                                  |
|----------------|----------------------------------------------------------|
| `GET /healthz` | Liveness — always `200` while the process is up          |
| `GET /readyz`  | Readiness — `503` when the queue is saturated; reports pool stats |
| `GET /v1/version` | Build + libvips version                               |
| `GET /metrics`    | Prometheus metrics (when enabled)                     |

## Configuration

`config/config.json` is a standard Drogon config plus a `custom_config` block:

| Key                | Default | Meaning                                               |
|--------------------|---------|-------------------------------------------------------|
| `workers`          | `0`     | Image worker threads (`0` = CPU core count)           |
| `max_queue`        | `256`   | Max queued jobs before shedding load (`503`)          |
| `vips_concurrency` | `1`     | libvips threads **per operation** (keep low)          |
| `max_image_pixels` | `1e8`   | Decoded-pixel cap (decompression-bomb guard; `0` off) |
| `security.api_keys` | `[]`   | API keys for `/v1/process`; empty = auth disabled     |
| `security.api_key_header` | `X-API-Key` | Header carrying the API key                  |
| `rate_limit.enabled` | `false` | Per-client token-bucket rate limiting              |
| `rate_limit.requests_per_sec` / `.burst` | `10` / `20` | Sustained rate / burst capacity |
| `cors.allow_origins` | `["*"]` | Allowed CORS origins (`["*"]` = any)               |
| `processing.job_timeout_ms` | `30000` | Per-request deadline (queue + compute); `0` off |
| `observability.metrics` | `true` | Serve `/metrics`                                  |
| `observability.access_log` | `true` | Per-request structured access logging          |

I/O threads, port, and max upload size live in the standard `app` / `listeners`
sections. See [docs/API.md](docs/API.md) for auth, rate-limit, and metrics details.

## Build from source

Requires a C++17 compiler, CMake ≥ 3.20, libvips (`libvips-dev`), and Drogon.

```bash
# System libvips
sudo apt-get install -y libvips-dev pkg-config   # Debian/Ubuntu

# Option A — Drogon via vcpkg (reproducible)
export VCPKG_ROOT=/path/to/vcpkg
cmake --preset default
cmake --build build -j

# Option B — Drogon already installed system-wide
cmake --preset system
cmake --build build -j

./build/image_server config/config.json
```

## Run with Docker

```bash
docker compose up --build
# or
docker build -t imageprocessingserver .
docker run --rm -p 8080:8080 imageprocessingserver
```

## Project layout

```
src/
  main.cpp                 # libvips init, config, worker pool, run loop
  version.h
  controllers/
    ProcessController.*     # POST /v1/process
    HealthController.*      # /healthz, /readyz, /v1/version
  core/
    JobQueue.*              # bounded worker pool (backpressure)
    ImageProcessor.*        # libvips transform pipeline
    ProcessOptions.*        # query-param parsing/validation
config/config.json
Dockerfile  docker-compose.yml  CMakeLists.txt  vcpkg.json
```

## Documentation

- [API reference](docs/API.md) — every endpoint, parameter, and error code
- [Features](docs/FEATURES.md) — capability + supported-format matrix
- [Roadmap](docs/ROADMAP.md) — planned milestones (v0.2 → v1.0)

## License

[MIT](LICENSE) © 2026 IoT Viet Solution.
