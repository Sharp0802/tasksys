#pragma once

#include <atomic>
#include <memory>
#include <cassert>
#include <utility>
#include <thread>

namespace ts {
  class Job {
    void (*_fn)(void *);
    void *_data;

  public:
    Job() : _fn(nullptr), _data(nullptr) {}
    Job(void (*fn)(void *), void *data) : _fn(fn), _data(data) {}

    void operator()() const {
      if (_fn) {
        _fn(_data);
      }
    }
  };

  /*
   * Chase-Lev deque implementation
   */
  class LocalQueue {
    size_t _mask;

    alignas(64) std::unique_ptr<Job[]> _array;
    alignas(64) std::atomic_size_t _head;
    alignas(64) std::atomic_size_t _tail;

  public:
    LocalQueue(const LocalQueue &) = delete;
    LocalQueue &operator=(const LocalQueue &) = delete;

    explicit LocalQueue(size_t size);

    bool pop(Job *job) noexcept;
    bool steal(Job *job) noexcept;
    bool push(const Job &job) noexcept;
  };


}