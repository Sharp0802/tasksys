#pragma once

#include "tasksys/LocalQueue.h"
#include "tasksys/GlobalQueue.h"
#include <condition_variable>
#include <cstddef>
#include <thread>
#include <unordered_map>
#include <vector>
#include <map>

namespace ts {

  class WorkerGroup;

  class Worker {
    WorkerGroup &_group;
    size_t _index;

    alignas(64) LocalQueue _queue;

    std::stop_token _token;
    std::jthread _thread;

    void run();
    bool steal(Job *job) noexcept;

  public:
    static thread_local Worker* tl_this_worker;

    Worker(WorkerGroup &group, size_t index, std::stop_source &sts, size_t size);

    void start() noexcept;
    bool push(const Job &job) noexcept;

#if PIN_WORKER
    bool pin(int cpu) noexcept;
#endif
    void join() noexcept;
  };

  class WorkerGroup {
    friend class Worker;

    Worker *_workers;
    int _size;
#if PIN_WORKER
    int _cpu_offset;
#else
    std::unordered_map<std::thread::id, Worker *> _worker_map;
#endif

    std::stop_source _sts;
    std::atomic_size_t _available;
    std::condition_variable _cv;
    std::mutex _mutex;
    std::atomic_bool _started;

    GlobalQueue _queue;

    void notify_closed() noexcept;
    void wait_for_start() noexcept;

#if PIN_WORKER
    [[nodiscard]] bool is_bound(int cpu) const noexcept;
    [[nodiscard]] int map_index(int cpu) const noexcept;
#endif

    void dispose() noexcept;

  public:
    WorkerGroup(int size, size_t capacity, size_t local_capacity);
    ~WorkerGroup();

    void start() noexcept;
    void push(const Job &job) noexcept;

    bool join(std::chrono::milliseconds timeout) noexcept;
    void stop() noexcept;
  };

}
