#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

namespace ips {

// Per-client token-bucket rate limiter. A client is identified by an opaque
// key (API key or IP). Tokens refill continuously at `ratePerSec` up to
// `burst` capacity; each allowed request consumes one token.
//
// Predictable, dependency-free, and thread-safe. Idle buckets are evicted
// lazily so the map does not grow unbounded.
class RateLimiterStore {
 public:
  static RateLimiterStore& instance();

  // Configure (or reconfigure) limits. ratePerSec<=0 or burst<=0 disables.
  void configure(double ratePerSec, int burst);

  // Returns true if the request for `clientKey` is permitted.
  bool allow(const std::string& clientKey);

 private:
  using Clock = std::chrono::steady_clock;

  struct Bucket {
    double tokens;
    Clock::time_point last;
  };

  RateLimiterStore() = default;
  void evictStale(Clock::time_point now);

  std::mutex mtx_;
  std::unordered_map<std::string, Bucket> buckets_;
  double ratePerSec_ = 0.0;
  double burst_ = 0.0;
  Clock::time_point lastSweep_{};
};

}  // namespace ips
