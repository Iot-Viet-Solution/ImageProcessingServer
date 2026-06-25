#include "core/ImageProcessor.h"

#include <vips/vips8>

#include <algorithm>
#include <atomic>
#include <string>

namespace ips {

using vips::VError;
using vips::VImage;
using vips::VOption;

namespace {

std::atomic<long long> g_maxPixels{0};

struct Encoding {
  std::string suffix;       // libvips writer suffix, e.g. ".webp"
  std::string contentType;  // HTTP content type
  bool lossy = false;       // honours the quality (Q) option
};

// Map an explicit `format` request to a libvips writer + content type.
Encoding encodingForFormat(const std::string& fmt) {
  if (fmt == "jpeg" || fmt == "jpg") return {".jpg", "image/jpeg", true};
  if (fmt == "png") return {".png", "image/png", false};
  if (fmt == "webp") return {".webp", "image/webp", true};
  if (fmt == "avif") return {".avif", "image/avif", true};
  if (fmt == "gif") return {".gif", "image/gif", false};
  if (fmt == "tiff" || fmt == "tif") return {".tif", "image/tiff", false};
  throw ImageError("unsupported format '" + fmt +
                   "'; supported: jpeg, png, webp, avif, gif, tiff");
}

// Detect the source format from the buffer so we can round-trip when the
// caller does not request an explicit output format.
Encoding detectEncoding(const std::string& input) {
  const char* loader =
      vips_foreign_find_load_buffer(input.data(), input.size());
  if (loader == nullptr) {
    throw ImageError("unrecognised or unsupported image format");
  }
  std::string l = loader;
  auto has = [&](const char* needle) {
    return l.find(needle) != std::string::npos;
  };
  if (has("Jpeg")) return {".jpg", "image/jpeg", true};
  if (has("Png")) return {".png", "image/png", false};
  if (has("WebP") || has("Webp")) return {".webp", "image/webp", true};
  if (has("Heif") || has("Avif")) return {".avif", "image/avif", true};
  if (has("Gif")) return {".gif", "image/gif", false};
  if (has("Tiff")) return {".tif", "image/tiff", false};
  // Fall back to PNG (lossless) for anything else decodable.
  return {".png", "image/png", false};
}

VImage loadResized(const std::string& input, const ProcessOptions& o) {
  // libvips thumbnail does shrink-on-load — far cheaper than decode-then-resize.
  // It targets a bounding box; we widen the unconstrained axis so a single
  // dimension behaves as "set that dimension, preserve aspect".
  const int kUnbounded = 1000000;
  int targetW = o.width.value_or(kUnbounded);
  int targetH = o.height.value_or(kUnbounded);
  int cropMode =
      (o.fit == Fit::Cover) ? VIPS_INTERESTING_CENTRE : VIPS_INTERESTING_NONE;

  // Use the libvips C API for thumbnailing: its signature has been stable since
  // 8.6, whereas the C++ VImage::thumbnail_buffer binding changed between 8.12
  // and 8.16. This still performs shrink-on-load.
  VipsImage* out = nullptr;
  if (vips_thumbnail_buffer(
          const_cast<void*>(static_cast<const void*>(input.data())),
          input.size(), &out, targetW, "height", targetH, "size",
          VIPS_SIZE_BOTH, "crop", cropMode, static_cast<void*>(nullptr))) {
    const char* err = vips_error_buffer();
    throw ImageError(std::string("resize failed: ") + (err ? err : "unknown"));
  }
  VImage img(out);  // VImage steals the reference and unrefs on destruction.

  // Fit::Fill ignores aspect ratio: stretch to the exact requested box.
  if (o.fit == Fit::Fill && o.width && o.height) {
    double hscale = static_cast<double>(*o.width) / img.width();
    double vscale = static_cast<double>(*o.height) / img.height();
    img = img.resize(hscale, VImage::option()->set("vscale", vscale));
  }
  return img;
}

VImage applyRotate(VImage img, int degrees) {
  int d = ((degrees % 360) + 360) % 360;
  switch (d) {
    case 0:
      return img;
    case 90:
      return img.rot(VIPS_ANGLE_D90);
    case 180:
      return img.rot(VIPS_ANGLE_D180);
    case 270:
      return img.rot(VIPS_ANGLE_D270);
    default:
      // Arbitrary angle (background defaults to transparent/black).
      return img.rotate(static_cast<double>(degrees));
  }
}

void guardPixels(const VImage& img) {
  long long cap = g_maxPixels.load();
  if (cap <= 0) return;
  long long pixels =
      static_cast<long long>(img.width()) * static_cast<long long>(img.height());
  if (pixels > cap) {
    throw ImageError("image exceeds the configured pixel limit");
  }
}

}  // namespace

void ImageProcessor::configure(long long maxPixels) {
  g_maxPixels.store(maxPixels);
}

ProcessedImage ImageProcessor::process(const std::string& input,
                                       const ProcessOptions& o) {
  if (input.empty()) {
    throw ImageError("empty request body: expected raw image bytes");
  }

  try {
    Encoding enc = o.format.empty() ? detectEncoding(input)
                                    : encodingForFormat(o.format);

    VImage img;
    const bool wantResize = o.width.has_value() || o.height.has_value();

    if (wantResize && !o.crop.has_value()) {
      img = loadResized(input, o);
    } else {
      // Random access (the default): operations such as 90/270 rotation and
      // arbitrary-angle rotation read pixels out of row order, which is
      // incompatible with VIPS_ACCESS_SEQUENTIAL streaming. The source is
      // already a fully in-memory buffer, so there is no streaming benefit to
      // give up here.
      img = VImage::new_from_buffer(
          input.data(), static_cast<size_t>(input.size()), "");

      if (o.crop) {
        const CropRect& c = *o.crop;
        if (c.x < 0 || c.y < 0 || c.x + c.w > img.width() ||
            c.y + c.h > img.height()) {
          throw ImageError("crop rectangle is outside the image bounds");
        }
        img = img.extract_area(c.x, c.y, c.w, c.h);
      }
      if (wantResize) {
        // Resize the (possibly cropped) region to the requested box.
        double hscale =
            o.width ? static_cast<double>(*o.width) / img.width() : 0.0;
        double vscale =
            o.height ? static_cast<double>(*o.height) / img.height() : 0.0;
        if (o.fit == Fit::Fill && o.width && o.height) {
          img = img.resize(hscale, VImage::option()->set("vscale", vscale));
        } else {
          double s = hscale > 0 && vscale > 0
                         ? (o.fit == Fit::Cover ? std::max(hscale, vscale)
                                                : std::min(hscale, vscale))
                         : (hscale > 0 ? hscale : vscale);
          img = img.resize(s);
        }
      }
    }

    guardPixels(img);

    if (o.rotate) img = applyRotate(img, *o.rotate);
    if (o.flip == "h" || o.flip == "hv" || o.flip == "vh") {
      img = img.flip(VIPS_DIRECTION_HORIZONTAL);
    }
    if (o.flip == "v" || o.flip == "hv" || o.flip == "vh") {
      img = img.flip(VIPS_DIRECTION_VERTICAL);
    }
    if (o.grayscale) img = img.colourspace(VIPS_INTERPRETATION_B_W);
    if (o.blur && *o.blur > 0) img = img.gaussblur(*o.blur);

    // Encode to the chosen format.
    VOption* wopt = VImage::option();
    if (enc.lossy && o.quality) wopt->set("Q", *o.quality);

    void* buf = nullptr;
    size_t len = 0;
    img.write_to_buffer(enc.suffix.c_str(), &buf, &len, wopt);

    ProcessedImage out;
    out.contentType = enc.contentType;
    out.data.assign(static_cast<char*>(buf), len);
    g_free(buf);
    return out;
  } catch (const ImageError&) {
    throw;
  } catch (const VError& e) {
    throw ImageError(std::string("image processing failed: ") + e.what());
  }
}

}  // namespace ips
