#pragma once

#include "worker.h"

namespace ts {
  constexpr size_t align(size_t n) {
    size_t r = 1;
    while (n > r) {
      r *= 2;
    }
    return r;
  }

  struct config {
    size_t worker_count = std::thread::hardware_concurrency();
    size_t local_queue_size = 4096;
    size_t local_batch_size = 256;
    size_t global_queue_size = align(4096 * worker_count);
  };

  class scheduler {
    config _config;
    std::vector<std::unique_ptr<worker>> _workers;
    vyukov<job*> _queue;

  public:
    explicit scheduler(const config &config) : _config(config), _queue(config.global_queue_size) {
      _workers.reserve(config.worker_count);
      for (size_t i = 0; i < config.worker_count; ++i) {
        _workers.emplace_back(
          std::make_unique<worker>(
            _workers,
            _queue,
            config.local_queue_size
          ));
      }
    }

    [[nodiscard]] const config &config() const noexcept { return _config; }

    void push(job *job) {
      if (const auto current = worker::current()) {
        current->push(job);
        return;
      }

      if (_queue.push(job)) {
        return;
      }

      // `blocking_push` cannot fail;
      // only way to fail is call `push` after `stop`.
#if NDEBUG
      _queue.blocking_push(job);
#else
      const auto r = _queue.blocking_push(job);
      assert(r);
#endif
    }

    [[nodiscard]]
    bool start() {
      for (size_t i = 0; i < _workers.size(); ++i) {
        if (!_workers[i]->start()) {
          _queue.kill();

          for (size_t j = 0; j < i; ++j) {
            _workers[j]->stop();
          }

          // _queue should have no item; doesn't flush items
          _queue.unsafe_reset();
          return false;
        }
      }

      return true;
    }

    void stop(const bool flush) {
      _queue.kill();

      for (const auto &worker : _workers) {
        worker->stop();
      }

      if (flush) {
        while (auto opt = _queue.pop()) {
          do {
            opt = opt.value()->call();
          } while (opt);
        }
      }

      _queue.unsafe_reset();
    }
  };
}
