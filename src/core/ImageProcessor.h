#pragma once

#include <stdexcept>
#include <string>

#include "core/ProcessOptions.h"

namespace ips {

struct ProcessedImage {
  std::string data;         // encoded image bytes
  std::string contentType;  // e.g. "image/webp"
};

// Thrown for client-correctable errors (bad/undecodable image, invalid op).
class ImageError : public std::runtime_error {
 public:
  explicit ImageError(const std::string& msg) : std::runtime_error(msg) {}
};

// Stateless image transform engine backed by libvips.
class ImageProcessor {
 public:
  // Configure global guard rails. maxPixels caps decoded width*height to
  // mitigate decompression-bomb inputs. 0 disables the cap.
  static void configure(long long maxPixels);

  // Decode `input`, apply the pipeline in `opts`, and re-encode.
  // Throws ImageError on invalid input or operations.
  static ProcessedImage process(const std::string& input,
                                const ProcessOptions& opts);
};

}  // namespace ips
