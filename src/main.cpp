#include <drogon/drogon.h>
#include <vips/vips8>

#include <chrono>
#include <thread>

#include "core/Config.h"
#include "core/ImageProcessor.h"
#include "core/JobQueue.h"
#include "core/Metrics.h"
#include "core/RateLimiter.h"
#include "core/RequestUtil.h"

using namespace drogon;

namespace {

// Add CORS headers, record metrics, and write an access-log line for every
// response. Runs globally via the post-handling AOP join-point.
void observabilityAdvice(const HttpRequestPtr& req,
                         const HttpResponsePtr& resp) {
  const ips::Config& cfg = ips::Config::get();

  const std::string& origin = req->getHeader("Origin");
  if (!origin.empty()) {
    std::string allow = cfg.corsAllowOrigin(origin);
    if (!allow.empty()) {
      resp->addHeader("Access-Control-Allow-Origin", allow);
      resp->addHeader("Vary", "Origin");
    }
  }

  const double durSec = static_cast<double>(
                            trantor::Date::now().microSecondsSinceEpoch() -
                            req->creationDate().microSecondsSinceEpoch()) /
                        1e6;
  const int status = static_cast<int>(resp->getStatusCode());
  ips::Metrics::instance().recordRequest(status, durSec);

  if (cfg.accessLog) {
    LOG_INFO << "access method=" << req->methodString()
             << " path=" << req->path() << " status=" << status
             << " bytes=" << resp->getBody().size() << " dur_ms="
             << durSec * 1000.0 << " ip=" << ips::clientIp(req);
  }
}

// Begin graceful shutdown: stop accepting new image work (readiness flips to
// 503), let in-flight jobs drain, then stop the event loop so their responses
// are still delivered.
void beginGracefulShutdown() {
  if (ips::Config::draining()) return;
  LOG_WARN << "shutdown signal received; draining in-flight jobs";
  ips::Config::setDraining(true);

  std::thread([] {
    using namespace std::chrono;
    auto& q = ips::JobQueue::instance();
    const auto deadline = steady_clock::now() + seconds(20);
    while (steady_clock::now() < deadline &&
           (q.pending() > 0 || q.inFlight() > 0)) {
      std::this_thread::sleep_for(milliseconds(50));
    }
    LOG_INFO << "drain complete; stopping event loop";
    app().quit();
  }).detach();
}

}  // namespace

int main(int argc, char* argv[]) {
  // libvips must be initialised before any image operation.
  if (VIPS_INIT(argv[0])) {
    vips_error_exit("failed to initialise libvips");
  }

  const char* configPath = (argc > 1) ? argv[1] : "config/config.json";
  app().loadConfigFile(configPath);

  ips::Config::load(app().getCustomConfig());
  const ips::Config& cfg = ips::Config::get();

  // We parallelise across requests via the worker pool, so keep libvips'
  // per-operation thread count low to avoid CPU oversubscription.
  vips_concurrency_set(cfg.vipsConcurrency);
  ips::ImageProcessor::configure(cfg.maxImagePixels);
  ips::JobQueue::init(cfg.workers, cfg.maxQueue);
  ips::RateLimiterStore::instance().configure(cfg.rateLimit.requestsPerSec,
                                              cfg.rateLimit.burst);

  app().registerPostHandlingAdvice(observabilityAdvice);
  app().setTermSignalHandler(beginGracefulShutdown);
  app().setIntSignalHandler(beginGracefulShutdown);

  LOG_INFO << "ImageProcessingServer starting: "
           << ips::JobQueue::instance().workerCount() << " image workers, queue "
           << cfg.maxQueue << ", auth=" << (cfg.authEnabled() ? "on" : "off")
           << ", rate_limit=" << (cfg.rateLimit.enabled ? "on" : "off")
           << ", timeout_ms=" << cfg.jobTimeoutMs << ", libvips "
           << vips_version_string();

  app().run();

  // Graceful shutdown: drain any remainder, then release libvips.
  ips::JobQueue::instance().stop();
  vips_shutdown();
  return 0;
}
