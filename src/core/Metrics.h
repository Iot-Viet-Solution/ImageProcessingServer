#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace ips {

// Minimal in-process metrics registry that renders the Prometheus text
// exposition format. Hot counters are atomic; low-frequency labelled maps are
// mutex-guarded (touched once per request).
class Metrics {
 public:
  static Metrics& instance();

  // Record a completed HTTP request: status code and total duration.
  void recordRequest(int statusCode, double durationSeconds);

  // Record the outcome of an image job: result in {ok, error, rejected,
  // timeout} and bytes produced (0 unless ok).
  void recordProcess(const std::string& result, std::uint64_t outputBytes);

  // Render the full exposition text. Live gauges (queue depth, workers) are
  // pulled from JobQueue at render time.
  std::string render() const;

 private:
  Metrics() = default;

  // Request-duration histogram buckets (seconds).
  static constexpr std::array<double, 11> kBuckets{
      0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0};

  mutable std::mutex mtx_;
  std::unordered_map<int, std::uint64_t> requestsByStatus_;
  std::array<std::uint64_t, kBuckets.size() + 1> durationBuckets_{};  // +inf
  double durationSum_ = 0.0;
  std::uint64_t durationCount_ = 0;

  std::unordered_map<std::string, std::uint64_t> processByResult_;
  std::atomic<std::uint64_t> outputBytesTotal_{0};
};

}  // namespace ips
