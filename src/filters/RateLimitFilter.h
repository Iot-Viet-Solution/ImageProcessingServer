#pragma once

#include <drogon/HttpFilter.h>

namespace ips {

// Per-client token-bucket rate limiting. No-op when disabled in config.
// Preflight OPTIONS requests always pass. Registered name:
// "ips::RateLimitFilter".
class RateLimitFilter : public drogon::HttpFilter<RateLimitFilter> {
 public:
  void doFilter(const drogon::HttpRequestPtr& req,
                drogon::FilterCallback&& fcb,
                drogon::FilterChainCallback&& fccb) override;
};

}  // namespace ips
