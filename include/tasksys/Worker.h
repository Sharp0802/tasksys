#pragma once

#include "tasksys/LocalQueue.h"
#include "tasksys/GlobalQueue.h"
#include <condition_variable>
#include <cstddef>
#include <thread>
#include <vector>
#include <map>

namespace ts {

  class WorkerGroup;

  class Worker {
    WorkerGroup &_group;
    size_t _index;

    LocalQueue _queue;

    std::stop_token _token;
    std::jthread _thread;

    void run();
    bool steal(Job *job) noexcept;

  public:
    Worker(WorkerGroup &group, size_t index, std::stop_source &sts, size_t size);

    bool push(const Job &job) noexcept;

    bool pin(int cpu) noexcept;
    void join() noexcept;
  };

  class WorkerGroup {
    friend class Worker;

    Worker *_workers;
    int _size;
    int _cpu_offset;

    std::atomic_bool _begin;
    std::stop_source _sts;
    std::atomic_size_t _available;
    std::condition_variable _cv;
    std::mutex _mutex;

    GlobalQueue _queue;

    void notify_closed() noexcept;

    [[nodiscard]] bool is_bound(int cpu) const noexcept;
    [[nodiscard]] int map_index(int cpu) const noexcept;

    void dispose() noexcept;

  public:
    WorkerGroup(int size, size_t capacity);
    ~WorkerGroup();

    void push(const Job &job) noexcept;

    bool join(std::chrono::milliseconds timeout) noexcept;
    void stop() noexcept;
  };

}
