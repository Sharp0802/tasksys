#pragma once

#include <stop_token>
#include <thread>
#include <vector>

#include "ChaseLevDeque.h"
#include "FAAQueue.h"

namespace ts {
  class WorkerGroup;

  class Worker {
    static constexpr int LOCAL_LIMIT = 512;
    static constexpr int GLOBAL_LIMIT = 64;
    static constexpr int STEAL_LIMIT = 64;

    std::stop_source _ss;

    size_t _id;
    std::vector<ChaseLevDeque> &_queues;
    FAAQueue &_global;
    WorkerGroup &_wg;

    std::jthread _thread;

    void run_job(Job job) const;

    ChaseLevDeque &steal_target() const;
    [[nodiscard]] bool steal_one() const;
    [[nodiscard]] bool process_one() const;
    [[nodiscard]] bool process_global_one() const;
    void run(const std::stop_token &token) const;

  public:
    Worker(size_t id, WorkerGroup &wg, std::vector<ChaseLevDeque> &queues, FAAQueue &global);
    ~Worker();

    void stop();
    void push(const Job &job) const;

    [[nodiscard]] std::thread::id thread_id() const;
  };
}
