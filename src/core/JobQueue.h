#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace ips {

// A bounded work queue backed by a fixed pool of worker threads.
//
// CPU-bound image processing is dispatched here so it never blocks Drogon's
// I/O event loops. When the queue is full, tryEnqueue() returns false and the
// caller is expected to reject the request (HTTP 503) — this is the server's
// backpressure mechanism.
class JobQueue {
 public:
  using Task = std::function<void()>;

  // Initialise the process-wide queue. workers==0 selects hardware concurrency.
  static void init(std::size_t workers, std::size_t maxQueue);
  static JobQueue& instance();

  // Returns false if the queue is at capacity (caller should shed load).
  bool tryEnqueue(Task task);

  std::size_t workerCount() const { return workers_.size(); }
  std::size_t maxQueue() const { return maxQueue_; }

  // Pending tasks not yet picked up by a worker.
  std::size_t pending();

  void stop();
  ~JobQueue();

  JobQueue(const JobQueue&) = delete;
  JobQueue& operator=(const JobQueue&) = delete;

 private:
  JobQueue(std::size_t workers, std::size_t maxQueue);
  void workerLoop();

  std::vector<std::thread> workers_;
  std::queue<Task> tasks_;
  std::mutex mtx_;
  std::condition_variable cv_;
  std::size_t maxQueue_;
  bool stopping_ = false;
};

}  // namespace ips
