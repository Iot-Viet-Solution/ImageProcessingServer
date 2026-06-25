#pragma once

#include <drogon/HttpController.h>

namespace ips {

// Liveness, readiness and version endpoints.
class HealthController : public drogon::HttpController<HealthController> {
 public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(HealthController::healthz, "/healthz", drogon::Get);
  ADD_METHOD_TO(HealthController::readyz, "/readyz", drogon::Get);
  ADD_METHOD_TO(HealthController::version, "/v1/version", drogon::Get);
  METHOD_LIST_END

  void healthz(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& callback);
  void readyz(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback);
  void version(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

}  // namespace ips
