#pragma once

#include <optional>
#include <string>

namespace drogon {
class HttpRequest;
}

namespace ips {

// How an image is fitted into the requested width/height box.
enum class Fit {
  Inside,   // preserve aspect, fit within box, never exceeding either side
  Outside,  // preserve aspect, cover box (largest side reaches the box)
  Cover,    // preserve aspect, fill box and centre-crop the overflow
  Fill,     // ignore aspect, stretch to exact width x height
};

struct CropRect {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
};

// A declarative description of the transform pipeline for one request,
// parsed from the query string.
struct ProcessOptions {
  std::optional<int> width;
  std::optional<int> height;
  Fit fit = Fit::Inside;

  std::optional<CropRect> crop;      // applied before resize
  std::optional<int> rotate;         // degrees, clockwise
  std::string flip;                  // "", "h", "v", "hv"
  bool grayscale = false;
  std::optional<double> blur;        // gaussian sigma

  std::string format;                // "", jpeg, png, webp, avif, gif, tiff
  std::optional<int> quality;        // 1..100 (lossy formats)

  // Parse options from request query parameters. Throws std::invalid_argument
  // on malformed values.
  static ProcessOptions fromRequest(const drogon::HttpRequest& req);
};

}  // namespace ips
