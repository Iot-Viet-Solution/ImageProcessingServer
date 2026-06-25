#pragma once

#include <drogon/HttpRequest.h>

#include <string>

#include "core/Config.h"

namespace ips {

// Extract the API key from the configured header or an `Authorization: Bearer`
// header. Returns "" if absent.
inline std::string apiKeyOf(const drogon::HttpRequestPtr& req,
                            const Config& cfg) {
  std::string key = req->getHeader(cfg.apiKeyHeader);
  if (key.empty()) {
    const std::string& auth = req->getHeader("Authorization");
    static const std::string kBearer = "Bearer ";
    if (auth.rfind(kBearer, 0) == 0) key = auth.substr(kBearer.size());
  }
  return key;
}

// Best-effort client IP: first hop of X-Forwarded-For if present, else the
// peer address.
inline std::string clientIp(const drogon::HttpRequestPtr& req) {
  const std::string& xff = req->getHeader("X-Forwarded-For");
  if (!xff.empty()) {
    auto comma = xff.find(',');
    std::string ip = (comma == std::string::npos) ? xff : xff.substr(0, comma);
    // trim spaces
    auto b = ip.find_first_not_of(" \t");
    auto e = ip.find_last_not_of(" \t");
    if (b != std::string::npos) return ip.substr(b, e - b + 1);
  }
  return req->getPeerAddr().toIp();
}

// Stable per-client identity for rate limiting: prefer the API key (so the
// limit follows the credential), else fall back to client IP.
inline std::string clientIdentity(const drogon::HttpRequestPtr& req,
                                  const Config& cfg) {
  if (cfg.authEnabled()) {
    std::string key = apiKeyOf(req, cfg);
    if (!key.empty()) return "key:" + key;
  }
  return "ip:" + clientIp(req);
}

}  // namespace ips
