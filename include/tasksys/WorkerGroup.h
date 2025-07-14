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
    explicit WorkerGroup(size_t size);
    ~WorkerGroup();

    void stop();
    void push(const Job &job);
  };
}
