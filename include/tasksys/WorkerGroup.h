#pragma once

#include <unordered_map>
#include <vector>

#include "ChaseLevDeque.h"
#include "Worker.h"

namespace ts {
  class WorkerGroup {
    std::vector<ChaseLevDeque> _queues;
    std::vector<Worker> _workers;
    std::unordered_map<std::thread::id, Worker*> _worker_map;

  public:
    explicit WorkerGroup(size_t size) : _queues(size) {
      _workers.reserve(size);
      for (size_t i = 0; i < size; i++) {
        auto &worker = _workers.emplace_back(i, _queues);
        _worker_map.emplace(worker.thread_id(), &worker);
      }
    }

    void push(const Job &job) {
      if (const auto iter = _worker_map.find(std::this_thread::get_id()); iter != _worker_map.end()) {
        iter->second->push(job);
        return;
      }

      // HERE: how to push job?
    }
  };
}
