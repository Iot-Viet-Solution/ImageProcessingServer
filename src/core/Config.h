#pragma once

#include <atomic>
#include <string>
#include <unordered_set>
#include <vector>

namespace Json {
class Value;
}

namespace ips {

struct RateLimitConfig {
  bool enabled = false;
  double requestsPerSec = 0.0;  // sustained rate per client
  int burst = 0;                // bucket capacity (max burst)
};

// Process-wide configuration parsed once from the `custom_config` block at
// startup, plus a runtime draining flag for graceful shutdown.
class Config {
 public:
  // Worker pool / engine (existing v0.1 settings).
  unsigned workers = 0;  // 0 = hardware concurrency
  unsigned maxQueue = 256;
  int vipsConcurrency = 1;
  long long maxImagePixels = 0;

  // v0.2 — security.
  std::unordered_set<std::string> apiKeys;       // empty => auth disabled
  std::string apiKeyHeader = "X-API-Key";

  // v0.2 — rate limiting.
  RateLimitConfig rateLimit;

  // v0.2 — CORS. "*" entry => allow any origin.
  std::vector<std::string> corsOrigins{"*"};

  // v0.2 — processing timeout (ms). 0 => disabled.
  long long jobTimeoutMs = 0;

  // v0.2 — observability.
  bool metricsEnabled = true;
  bool accessLog = true;

  bool authEnabled() const { return !apiKeys.empty(); }
  // Returns the value to use for Access-Control-Allow-Origin, or "" if the
  // origin is not allowed.
  std::string corsAllowOrigin(const std::string& requestOrigin) const;

  // Parse and install the global config. Call once at startup.
  static void load(const Json::Value& custom);
  static const Config& get();

  // Graceful-shutdown drain flag (set on SIGTERM/SIGINT).
  static void setDraining(bool v) { draining_.store(v); }
  static bool draining() { return draining_.load(); }

 private:
  static std::atomic<bool> draining_;
};

}  // namespace ips
