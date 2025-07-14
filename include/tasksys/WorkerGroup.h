#pragma once

#include <unordered_map>
#include <vector>

#include "ChaseLevDeque.h"
#include "Worker.h"

namespace ts {
  class WorkerGroup {
    std::vector<ChaseLevDeque> _queues;
    FAAQueue _global;
    std::unordered_map<std::thread::id, Worker*> _worker_map;
    std::vector<Worker> _workers;

  public:
    explicit WorkerGroup(size_t size) : _queues(size) {
      _workers.reserve(size);
      for (size_t i = 0; i < size; i++) {
        _workers.emplace_back(i, _queues, _global);
      }

      for (auto &worker : _workers) {
        _worker_map.emplace(worker.thread_id(), &worker);
      }
    }

    ~WorkerGroup() {
      stop();
    }

    void stop() {
      for (auto &worker : _workers) {
        worker.stop();
      }
    }

    void push(const Job &job) {
      if (const auto iter = _worker_map.find(std::this_thread::get_id()); iter != _worker_map.end()) {
        iter->second->push(job);
        return;
      }

      while (!_global.push(job)) {
        std::this_thread::yield();
      }
    }
  };
}
