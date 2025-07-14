#include "tasksys/WorkerGroup.h"

namespace ts {
  WorkerGroup::WorkerGroup(size_t size): _queues(size) {
    _workers.reserve(size);
    for (size_t i = 0; i < size; i++) {
      _workers.emplace_back(i, _queues, _global);
    }

    for (auto &worker : _workers) {
      _worker_map.emplace(worker.thread_id(), &worker);
    }
  }

  WorkerGroup::~WorkerGroup() {
    stop();
  }

  void WorkerGroup::stop() {
    for (auto &worker : _workers) {
      worker.stop();
    }
  }

  void WorkerGroup::push(const Job &job) {
    if (const auto iter = _worker_map.find(std::this_thread::get_id()); iter != _worker_map.end()) {
      iter->second->push(job);
      return;
    }

    while (!_global.push(job)) {
      std::this_thread::yield();
    }
  }
}
