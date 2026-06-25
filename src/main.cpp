#include <drogon/drogon.h>
#include <vips/vips8>

#include <csignal>
#include <cstddef>
#include <thread>

#include "core/ImageProcessor.h"
#include "core/JobQueue.h"

namespace {

std::size_t resolveWorkers(const Json::Value& custom) {
  unsigned w = custom.get("workers", 0u).asUInt();
  if (w == 0) {
    w = std::thread::hardware_concurrency();
    if (w == 0) w = 2;
  }
  return w;
}

}  // namespace

int main(int argc, char* argv[]) {
  // libvips must be initialised before any image operation and shut down at
  // exit. The argv[0] hint lets it locate its module/cache directories.
  if (VIPS_INIT(argv[0])) {
    vips_error_exit("failed to initialise libvips");
  }

  const char* configPath = (argc > 1) ? argv[1] : "config/config.json";
  drogon::app().loadConfigFile(configPath);

  const Json::Value custom = drogon::app().getCustomConfig();

  const std::size_t workers = resolveWorkers(custom);
  const std::size_t maxQueue = custom.get("max_queue", 256u).asUInt();
  const int vipsConcurrency = custom.get("vips_concurrency", 1).asInt();
  const long long maxPixels =
      custom.get("max_image_pixels", 0).asLargestInt();

  // We parallelise across requests via the worker pool, so keep libvips'
  // per-operation thread count low to avoid CPU oversubscription.
  vips_concurrency_set(vipsConcurrency);

  ips::ImageProcessor::configure(maxPixels);
  ips::JobQueue::init(workers, maxQueue);

  LOG_INFO << "ImageProcessingServer starting: " << workers
           << " image workers, queue capacity " << maxQueue
           << ", libvips " << vips_version_string();

  drogon::app().run();

  // Graceful shutdown.
  ips::JobQueue::instance().stop();
  vips_shutdown();
  return 0;
}
