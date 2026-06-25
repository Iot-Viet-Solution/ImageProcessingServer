#include "filters/RateLimitFilter.h"

#include "core/Config.h"
#include "core/RateLimiter.h"
#include "core/RequestUtil.h"

using namespace drogon;

namespace ips {

void RateLimitFilter::doFilter(const HttpRequestPtr& req, FilterCallback&& fcb,
                               FilterChainCallback&& fccb) {
  if (req->method() == Options) {
    fccb();
    return;
  }

  const Config& cfg = Config::get();
  if (!cfg.rateLimit.enabled) {
    fccb();
    return;
  }

  if (RateLimiterStore::instance().allow(clientIdentity(req, cfg))) {
    fccb();
    return;
  }

  Json::Value err;
  err["error"] = "rate limit exceeded";
  auto resp = HttpResponse::newHttpJsonResponse(err);
  resp->setStatusCode(k429TooManyRequests);
  resp->addHeader("Retry-After", "1");
  fcb(resp);
}

}  // namespace ips
