#include "core/Config.h"

#include <json/json.h>

#include <algorithm>

namespace ips {

namespace {
Config g_config;
}

std::atomic<bool> Config::draining_{false};

std::string Config::corsAllowOrigin(const std::string& requestOrigin) const {
  for (const auto& o : corsOrigins) {
    if (o == "*") return "*";
    if (o == requestOrigin && !requestOrigin.empty()) return requestOrigin;
  }
  return "";
}

void Config::load(const Json::Value& c) {
  Config cfg;

  // Worker pool / engine.
  cfg.workers = c.get("workers", 0u).asUInt();
  cfg.maxQueue = c.get("max_queue", 256u).asUInt();
  cfg.vipsConcurrency = c.get("vips_concurrency", 1).asInt();
  cfg.maxImagePixels = c.get("max_image_pixels", 0).asLargestInt();

  // Security.
  const Json::Value& sec = c["security"];
  if (sec.isObject()) {
    if (sec["api_keys"].isArray()) {
      for (const auto& k : sec["api_keys"]) {
        std::string key = k.asString();
        if (!key.empty()) cfg.apiKeys.insert(key);
      }
    }
    cfg.apiKeyHeader = sec.get("api_key_header", "X-API-Key").asString();
  }

  // Rate limiting.
  const Json::Value& rl = c["rate_limit"];
  if (rl.isObject()) {
    cfg.rateLimit.enabled = rl.get("enabled", false).asBool();
    cfg.rateLimit.requestsPerSec = rl.get("requests_per_sec", 0.0).asDouble();
    cfg.rateLimit.burst = rl.get("burst", 0).asInt();
  }

  // CORS.
  const Json::Value& cors = c["cors"];
  if (cors.isObject() && cors["allow_origins"].isArray() &&
      !cors["allow_origins"].empty()) {
    cfg.corsOrigins.clear();
    for (const auto& o : cors["allow_origins"]) {
      cfg.corsOrigins.push_back(o.asString());
    }
  }

  // Processing.
  const Json::Value& proc = c["processing"];
  if (proc.isObject()) {
    cfg.jobTimeoutMs = proc.get("job_timeout_ms", 0).asLargestInt();
  }

  // Observability.
  const Json::Value& obs = c["observability"];
  if (obs.isObject()) {
    cfg.metricsEnabled = obs.get("metrics", true).asBool();
    cfg.accessLog = obs.get("access_log", true).asBool();
  }

  g_config = std::move(cfg);
}

const Config& Config::get() { return g_config; }

}  // namespace ips
