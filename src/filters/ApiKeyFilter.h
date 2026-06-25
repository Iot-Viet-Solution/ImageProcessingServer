#pragma once

#include <drogon/HttpFilter.h>

namespace ips {

// Rejects requests that lack a valid API key. No-op when no keys are
// configured (auth disabled). Preflight OPTIONS requests always pass so CORS
// works. Registered name: "ips::ApiKeyFilter".
class ApiKeyFilter : public drogon::HttpFilter<ApiKeyFilter> {
 public:
  void doFilter(const drogon::HttpRequestPtr& req,
                drogon::FilterCallback&& fcb,
                drogon::FilterChainCallback&& fccb) override;
};

}  // namespace ips
