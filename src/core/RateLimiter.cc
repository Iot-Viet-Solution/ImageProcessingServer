#include "core/RateLimiter.h"

#include <algorithm>

namespace ips {

RateLimiterStore& RateLimiterStore::instance() {
  static RateLimiterStore s;
  return s;
}

void RateLimiterStore::configure(double ratePerSec, int burst) {
  std::lock_guard<std::mutex> lock(mtx_);
  ratePerSec_ = ratePerSec;
  burst_ = static_cast<double>(burst);
  buckets_.clear();
}

void RateLimiterStore::evictStale(Clock::time_point now) {
  // Drop buckets that have been full (idle) for a while to bound memory.
  // Called while holding mtx_.
  for (auto it = buckets_.begin(); it != buckets_.end();) {
    double elapsed =
        std::chrono::duration<double>(now - it->second.last).count();
    if (elapsed > 60.0) {
      it = buckets_.erase(it);
    } else {
      ++it;
    }
  }
  lastSweep_ = now;
}

bool RateLimiterStore::allow(const std::string& clientKey) {
  std::lock_guard<std::mutex> lock(mtx_);
  if (ratePerSec_ <= 0.0 || burst_ <= 0.0) return true;  // disabled

  const auto now = Clock::now();

  if (std::chrono::duration<double>(now - lastSweep_).count() > 30.0) {
    evictStale(now);
  }

  auto it = buckets_.find(clientKey);
  if (it == buckets_.end()) {
    // New client starts with a full bucket, then consumes one token.
    buckets_.emplace(clientKey, Bucket{burst_ - 1.0, now});
    return true;
  }

  Bucket& b = it->second;
  double elapsed = std::chrono::duration<double>(now - b.last).count();
  b.tokens = std::min(burst_, b.tokens + elapsed * ratePerSec_);
  b.last = now;

  if (b.tokens >= 1.0) {
    b.tokens -= 1.0;
    return true;
  }
  return false;
}

}  // namespace ips
