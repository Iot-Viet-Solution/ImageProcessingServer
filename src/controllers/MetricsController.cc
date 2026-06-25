#include "controllers/MetricsController.h"

#include "core/Config.h"
#include "core/Metrics.h"

using namespace drogon;

namespace ips {

void MetricsController::metrics(
    const HttpRequestPtr&,
    std::function<void(const HttpResponsePtr&)>&& callback) {
  if (!Config::get().metricsEnabled) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k404NotFound);
    resp->setBody("metrics disabled\n");
    callback(resp);
    return;
  }
  auto resp = HttpResponse::newHttpResponse();
  resp->setBody(Metrics::instance().render());
  // Prometheus text exposition format, version 0.0.4.
  resp->setContentTypeString("text/plain; version=0.0.4; charset=utf-8");
  callback(resp);
}

}  // namespace ips
