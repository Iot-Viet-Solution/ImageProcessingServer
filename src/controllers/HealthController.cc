#include "controllers/HealthController.h"

#include <vips/vips8>

#include "core/Config.h"
#include "core/JobQueue.h"
#include "version.h"

using namespace drogon;

namespace ips {

void HealthController::healthz(
    const HttpRequestPtr&,
    std::function<void(const HttpResponsePtr&)>&& callback) {
  Json::Value body;
  body["status"] = "ok";
  callback(HttpResponse::newHttpJsonResponse(body));
}

void HealthController::readyz(
    const HttpRequestPtr&,
    std::function<void(const HttpResponsePtr&)>&& callback) {
  auto& q = JobQueue::instance();
  std::size_t pending = q.pending();
  const bool draining = Config::draining();
  const bool saturated = pending >= q.maxQueue();

  Json::Value body;
  body["status"] = draining ? "draining" : (saturated ? "saturated" : "ready");
  body["workers"] = static_cast<Json::UInt64>(q.workerCount());
  body["queue_pending"] = static_cast<Json::UInt64>(pending);
  body["queue_inflight"] = static_cast<Json::UInt64>(q.inFlight());
  body["queue_capacity"] = static_cast<Json::UInt64>(q.maxQueue());

  auto resp = HttpResponse::newHttpJsonResponse(body);
  if (draining || saturated) resp->setStatusCode(k503ServiceUnavailable);
  callback(resp);
}

void HealthController::version(
    const HttpRequestPtr&,
    std::function<void(const HttpResponsePtr&)>&& callback) {
  Json::Value body;
  body["name"] = "ImageProcessingServer";
  body["version"] = IPS_VERSION;
  body["engine"] = "libvips";
  body["libvips"] = vips_version_string();
  callback(HttpResponse::newHttpJsonResponse(body));
}

}  // namespace ips
