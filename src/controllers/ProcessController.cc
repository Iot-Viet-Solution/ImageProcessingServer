#include "controllers/ProcessController.h"

#include <drogon/MultiPart.h>

#include <chrono>
#include <string>
#include <utility>

#include "core/Config.h"
#include "core/ImageProcessor.h"
#include "core/JobQueue.h"
#include "core/Metrics.h"
#include "core/ProcessOptions.h"

using namespace drogon;

namespace ips {

namespace {

HttpResponsePtr jsonError(HttpStatusCode code, const std::string& message) {
  Json::Value body;
  body["error"] = message;
  auto resp = HttpResponse::newHttpJsonResponse(body);
  resp->setStatusCode(code);
  return resp;
}

// Extract image bytes from either a raw body or a multipart upload.
// Returns false if no image payload could be found.
bool extractImage(const HttpRequestPtr& req, std::string& out) {
  const std::string& ct = req->getHeader("content-type");
  if (ct.find("multipart/form-data") != std::string::npos) {
    MultiPartParser parser;
    if (parser.parse(req) != 0) return false;
    const auto& files = parser.getFiles();
    if (files.empty()) return false;
    for (const auto& f : files) {
      const std::string& name = f.getItemName();
      if (name == "image" || name == "file" || files.size() == 1) {
        auto content = f.fileContent();
        out.assign(content.data(), content.size());
        return !out.empty();
      }
    }
    return false;
  }

  // Raw binary body.
  auto body = req->getBody();
  out.assign(body.data(), body.size());
  return !out.empty();
}

}  // namespace

void ProcessController::process(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {
  // CORS preflight. The Allow-Origin header is added globally by the
  // post-handling advice; here we just answer the method/header negotiation.
  if (req->method() == Options) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k204NoContent);
    resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers",
                    "Content-Type, X-API-Key, Authorization");
    resp->addHeader("Access-Control-Max-Age", "86400");
    callback(resp);
    return;
  }

  // Shed new work while draining for graceful shutdown.
  if (Config::draining()) {
    Metrics::instance().recordProcess("rejected", 0);
    auto resp = jsonError(k503ServiceUnavailable, "server is shutting down");
    resp->addHeader("Retry-After", "5");
    callback(resp);
    return;
  }

  ProcessOptions opts;
  try {
    opts = ProcessOptions::fromRequest(*req);
  } catch (const std::exception& e) {
    callback(jsonError(k400BadRequest, e.what()));
    return;
  }

  auto image = std::make_shared<std::string>();
  if (!extractImage(req, *image)) {
    callback(jsonError(
        k400BadRequest,
        "no image found: send raw image bytes or a multipart 'image' field"));
    return;
  }

  // Per-request processing deadline (queue wait + compute), if configured.
  const long long timeoutMs = Config::get().jobTimeoutMs;
  const auto deadline =
      timeoutMs > 0 ? std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeoutMs)
                    : std::chrono::steady_clock::time_point::max();

  // Hand the CPU-bound work to the worker pool. If the queue is saturated we
  // shed load immediately rather than letting latency balloon.
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr&)>>(
      std::move(callback));

  bool accepted = JobQueue::instance().tryEnqueue([image, opts, cb, deadline]() {
    auto& metrics = Metrics::instance();
    try {
      ProcessedImage result = ImageProcessor::process(*image, opts, deadline);
      auto resp = HttpResponse::newHttpResponse();
      const auto bytes = result.data.size();
      resp->setBody(std::move(result.data));
      resp->setContentTypeString(result.contentType);
      resp->addHeader("Cache-Control", "no-store");
      metrics.recordProcess("ok", bytes);
      (*cb)(resp);
    } catch (const ImageTimeout& e) {
      metrics.recordProcess("timeout", 0);
      (*cb)(jsonError(k504GatewayTimeout, e.what()));
    } catch (const ImageError& e) {
      metrics.recordProcess("error", 0);
      (*cb)(jsonError(k422UnprocessableEntity, e.what()));
    } catch (const std::exception& e) {
      metrics.recordProcess("error", 0);
      (*cb)(jsonError(k500InternalServerError,
                      std::string("internal error: ") + e.what()));
    }
  });

  if (!accepted) {
    Metrics::instance().recordProcess("rejected", 0);
    auto resp = jsonError(k503ServiceUnavailable,
                          "server busy: image queue is full, retry shortly");
    resp->addHeader("Retry-After", "1");
    (*cb)(resp);
  }
}

}  // namespace ips
