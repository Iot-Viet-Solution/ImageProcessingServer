#include "filters/ApiKeyFilter.h"

#include "core/Config.h"
#include "core/RequestUtil.h"

using namespace drogon;

namespace ips {

void ApiKeyFilter::doFilter(const HttpRequestPtr& req, FilterCallback&& fcb,
                            FilterChainCallback&& fccb) {
  if (req->method() == Options) {  // allow CORS preflight through
    fccb();
    return;
  }

  const Config& cfg = Config::get();
  if (!cfg.authEnabled()) {  // auth disabled
    fccb();
    return;
  }

  const std::string key = apiKeyOf(req, cfg);
  if (!key.empty() && cfg.apiKeys.count(key) > 0) {
    fccb();
    return;
  }

  Json::Value err;
  err["error"] = "unauthorized: a valid API key is required";
  auto resp = HttpResponse::newHttpJsonResponse(err);
  resp->setStatusCode(k401Unauthorized);
  resp->addHeader("WWW-Authenticate", "Bearer");
  fcb(resp);
}

}  // namespace ips
