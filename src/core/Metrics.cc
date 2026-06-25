#include "core/Metrics.h"

#include <sstream>

#include "core/JobQueue.h"
#include "version.h"

namespace ips {

constexpr std::array<double, 11> Metrics::kBuckets;

Metrics& Metrics::instance() {
  static Metrics m;
  return m;
}

void Metrics::recordRequest(int statusCode, double durationSeconds) {
  std::lock_guard<std::mutex> lock(mtx_);
  ++requestsByStatus_[statusCode];
  durationSum_ += durationSeconds;
  ++durationCount_;
  for (std::size_t i = 0; i < kBuckets.size(); ++i) {
    if (durationSeconds <= kBuckets[i]) ++durationBuckets_[i];
  }
  ++durationBuckets_[kBuckets.size()];  // +Inf
}

void Metrics::recordProcess(const std::string& result,
                            std::uint64_t outputBytes) {
  if (outputBytes) outputBytesTotal_.fetch_add(outputBytes);
  std::lock_guard<std::mutex> lock(mtx_);
  ++processByResult_[result];
}

std::string Metrics::render() const {
  std::ostringstream os;

  os << "# HELP ips_build_info Build information.\n"
     << "# TYPE ips_build_info gauge\n"
     << "ips_build_info{version=\"" << IPS_VERSION << "\"} 1\n";

  std::lock_guard<std::mutex> lock(mtx_);

  os << "# HELP ips_http_requests_total Total HTTP requests by status code.\n"
     << "# TYPE ips_http_requests_total counter\n";
  for (const auto& [code, count] : requestsByStatus_) {
    os << "ips_http_requests_total{status=\"" << code << "\"} " << count << "\n";
  }

  os << "# HELP ips_http_request_duration_seconds Request duration histogram.\n"
     << "# TYPE ips_http_request_duration_seconds histogram\n";
  for (std::size_t i = 0; i < kBuckets.size(); ++i) {
    os << "ips_http_request_duration_seconds_bucket{le=\"" << kBuckets[i]
       << "\"} " << durationBuckets_[i] << "\n";
  }
  os << "ips_http_request_duration_seconds_bucket{le=\"+Inf\"} "
     << durationBuckets_[kBuckets.size()] << "\n";
  os << "ips_http_request_duration_seconds_sum " << durationSum_ << "\n";
  os << "ips_http_request_duration_seconds_count " << durationCount_ << "\n";

  os << "# HELP ips_process_total Image jobs by result.\n"
     << "# TYPE ips_process_total counter\n";
  for (const auto& [result, count] : processByResult_) {
    os << "ips_process_total{result=\"" << result << "\"} " << count << "\n";
  }

  os << "# HELP ips_output_bytes_total Total encoded output bytes.\n"
     << "# TYPE ips_output_bytes_total counter\n"
     << "ips_output_bytes_total " << outputBytesTotal_.load() << "\n";

  // Live worker-pool gauges.
  auto& q = JobQueue::instance();
  os << "# HELP ips_queue_pending Jobs queued but not yet started.\n"
     << "# TYPE ips_queue_pending gauge\n"
     << "ips_queue_pending " << q.pending() << "\n"
     << "# HELP ips_queue_inflight Jobs currently being processed.\n"
     << "# TYPE ips_queue_inflight gauge\n"
     << "ips_queue_inflight " << q.inFlight() << "\n"
     << "# HELP ips_queue_capacity Maximum queue depth before load shedding.\n"
     << "# TYPE ips_queue_capacity gauge\n"
     << "ips_queue_capacity " << q.maxQueue() << "\n"
     << "# HELP ips_workers Number of image worker threads.\n"
     << "# TYPE ips_workers gauge\n"
     << "ips_workers " << q.workerCount() << "\n";

  return os.str();
}

}  // namespace ips
