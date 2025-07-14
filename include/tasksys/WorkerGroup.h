#pragma once

#include <unordered_map>
#include <vector>

#include "ChaseLevDeque.h"
#include "Worker.h"

namespace ts {
  class WorkerGroup {
    size_t _worker_count;
    std::vector<ChaseLevDeque> _queues;
    FAAQueue _global;
    std::unordered_map<std::thread::id, Worker*> _worker_map;
    Worker* _workers;

  public:
    WorkerGroup(const WorkerGroup &) = delete;
    WorkerGroup &operator=(const WorkerGroup &) = delete;

    explicit WorkerGroup(size_t size);
    ~WorkerGroup();

    void stop();
    void push(const Job &job);
  };
}
