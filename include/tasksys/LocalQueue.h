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

    template<typename Fn, typename T>
    Job(Fn&& fn, T *data) : _fn(reinterpret_cast<void (*)(void *)>(static_cast<void(*)(T*)>(fn))), _data(data) {}

    operator bool() const {
      return _fn;
    }

    void operator()() const {
      if (_fn) {
        _fn(_data);
      }
    }

    bool operator==(const Job &other) const {
      return other._fn == _fn && other._data == _data;
    }

    bool operator!=(const Job &other) const {
      return other._fn != _fn || other._data != _data;
    }
  };

  /*
   * Chase-Lev deque implementation
   */
  class LocalQueue {
    size_t _mask;

    alignas(64) Job *_array;
    alignas(64) std::atomic_size_t _head;
    alignas(64) std::atomic_size_t _tail;

  public:
    LocalQueue(const LocalQueue &) = delete;
    LocalQueue &operator=(const LocalQueue &) = delete;

    explicit LocalQueue(size_t size);
    ~LocalQueue();

    bool pop(Job *job) noexcept;
    bool steal(Job *job) noexcept;
    bool push(const Job &job) noexcept;
  };


}