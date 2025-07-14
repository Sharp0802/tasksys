#pragma once

#include <stop_token>
#include <thread>
#include <vector>

#include "ChaseLevDeque.h"

namespace ts {
  class Worker {
    static constexpr int SPIN_LIMIT = 512;
    static constexpr int STEAL_LIMIT = 64;

    std::stop_source _ss;

    size_t _id;
    size_t _steal_id;
    std::vector<ChaseLevDeque> &_queues;

    std::jthread _thread;


    [[nodiscard]]
    bool process_one() const;
    ChaseLevDeque &steal_target();
    bool steal_one();
    void run(const std::stop_token &token);

  public:
    Worker(size_t id, std::vector<ChaseLevDeque> &queues);
    ~Worker();

    void stop();
    void push(const Job &job) const;
  };
}
