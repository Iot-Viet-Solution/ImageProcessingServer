#include "core/JobQueue.h"

#include <memory>
#include <utility>

namespace ips {

namespace {
std::unique_ptr<JobQueue> g_instance;
}

JobQueue::JobQueue(std::size_t workers, std::size_t maxQueue)
    : maxQueue_(maxQueue == 0 ? 1 : maxQueue) {
  if (workers == 0) {
    workers = std::thread::hardware_concurrency();
    if (workers == 0) workers = 2;
  }
  workers_.reserve(workers);
  for (std::size_t i = 0; i < workers; ++i) {
    workers_.emplace_back([this] { workerLoop(); });
  }
}

JobQueue::~JobQueue() { stop(); }

void JobQueue::init(std::size_t workers, std::size_t maxQueue) {
  g_instance.reset(new JobQueue(workers, maxQueue));
}

JobQueue& JobQueue::instance() { return *g_instance; }

bool JobQueue::tryEnqueue(Task task) {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    if (stopping_) return false;
    if (tasks_.size() >= maxQueue_) return false;  // shed load
    tasks_.push(std::move(task));
  }
  cv_.notify_one();
  return true;
}

std::size_t JobQueue::pending() {
  std::lock_guard<std::mutex> lock(mtx_);
  return tasks_.size();
}

void JobQueue::workerLoop() {
  for (;;) {
    Task task;
    {
      std::unique_lock<std::mutex> lock(mtx_);
      cv_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
      if (stopping_ && tasks_.empty()) return;
      task = std::move(tasks_.front());
      tasks_.pop();
    }
    task();  // exceptions are handled inside the task itself
  }
}

void JobQueue::stop() {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    if (stopping_) return;
    stopping_ = true;
  }
  cv_.notify_all();
  for (auto& t : workers_) {
    if (t.joinable()) t.join();
  }
  workers_.clear();
}

}  // namespace ips
