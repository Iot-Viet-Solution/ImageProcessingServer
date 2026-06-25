#include "controllers/ProcessController.h"

#include <drogon/MultiPart.h>

#include <string>
#include <utility>

#include "core/ImageProcessor.h"
#include "core/JobQueue.h"
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
  // CORS / preflight.
  if (req->method() == Options) {
    auto resp = HttpResponse::newHttpResponse();
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
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

  // Hand the CPU-bound work to the worker pool. If the queue is saturated we
  // shed load immediately rather than letting latency balloon.
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr&)>>(
      std::move(callback));

  bool accepted = JobQueue::instance().tryEnqueue([image, opts, cb]() {
    try {
      ProcessedImage result = ImageProcessor::process(*image, opts);
      auto resp = HttpResponse::newHttpResponse();
      resp->setBody(std::move(result.data));
      resp->setContentTypeString(result.contentType);
      resp->addHeader("Access-Control-Allow-Origin", "*");
      resp->addHeader("Cache-Control", "no-store");
      (*cb)(resp);
    } catch (const ImageError& e) {
      (*cb)(jsonError(k422UnprocessableEntity, e.what()));
    } catch (const std::exception& e) {
      (*cb)(jsonError(k500InternalServerError,
                      std::string("internal error: ") + e.what()));
    }
  });

  if (!accepted) {
    auto resp = jsonError(k503ServiceUnavailable,
                          "server busy: image queue is full, retry shortly");
    resp->addHeader("Retry-After", "1");
    (*cb)(resp);
  }
}

}  // namespace ips
