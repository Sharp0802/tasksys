#pragma once

#include <chrono>
#include <thread>

#include "job.h"
#include "queue.h"

namespace ts {
  inline uint32_t rnd32() {
    thread_local uint32_t state =
      std::hash<std::thread::id>{}(std::this_thread::get_id())
      ^ std::chrono::high_resolution_clock::now().time_since_epoch().count();
    if (state == 0) {
      state = 0x9e3779b9u;
    }

    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;

    return state;
  }

  inline void cpu_relax() noexcept {
#if defined(_MSC_VER) || __clang__
    _mm_pause();
#elif defined(__i386__) || defined(__x86_64__)
    __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
    __asm__ __volatile__("yield" ::: "memory");
#else
    std::this_thread::yield();
#endif
  }


  class worker {
    std::atomic_flag _active = ATOMIC_FLAG_INIT;

    const std::vector<worker*> &_workers;
    size_t _id;

    vyukov<job*> &_global;

    pool<job> _storage;
    chaselev<job*> _local;

    size_t _batch_size;

    std::optional<std::jthread> _thread;

    // note: job must be dynamically allocated
    job *chunk(job *job) {
      while (job->size() > _batch_size) {
        auto [left, right] = job->split(job->size() / 2);

        // reuse space
        *job = std::move(right);
        _local.push(job);

        job = _storage.rent(std::move(left));
      }

      return job;
    }

    job *take() {
      if (const auto opt = _local.take()) {
        return opt.value();
      }

      if (_workers.size() > 1) {

        auto ofs = rnd32() % (_workers.size() - 1);
        ofs = (_id + ofs) % _workers.size();

        if (const auto opt = _workers[ofs]->_local.steal()) {
          return opt.value();
        }
      }

      if (const auto opt = _global.pop()) {
        return opt.value();
      }

      return nullptr;
    }

    void loop() {
      size_t miss = 0;
      while (_active.test()) {
        job *job = take();
        if (!job) {
          if (miss < 2000) {
            miss++;
            cpu_relax();
            continue;
          }

          if (miss < 10000) {
            miss++;
            std::this_thread::yield();
            continue;
          }

          if (auto opt = _global.blocking_pop()) {
            job = opt.value();
          } else {
            continue;
          }
        }

        miss = 0;

        std::optional<ts::job*> opt;
        do {
          opt = chunk(job)->call();
          _storage.yield(job);
        } while (opt);
      }
    }

  public:
    worker(
      const std::vector<worker*> &workers,
      vyukov<job*> &global,
      const size_t size,
      const size_t batch_size)
      : _workers(workers),
        _id(-1),
        _global(global),
        _local(size),
        _batch_size(batch_size) {
      assert(_batch_size > 0);
    }

    void push(job *job) {
      if (_local.push(job)) {
        return;
      }

      if (_global.push(job)) {
        return;
      }

      _global.blocking_push(job);
    }

    bool start() {
      if (_active.test_and_set()) {
        return false;
      }

      _id = -1;
      for (auto i = 0; i < _workers.size(); i++) {
        if (_workers[i] == this) {
          _id = i;
        }
      }
      if (_id == -1) {
        throw std::invalid_argument("Worker must be initialized with worker list that contains self");
      }

      _thread.emplace([this] {
        loop();
      });

      return true;
    }

    void stop() {
      if (!_active.test()) {
        return;
      }

      _active.clear();
      if (_thread->joinable()) {
        _thread->join();
      }
    }

    ~worker() {
      stop();
    }
  };
}
