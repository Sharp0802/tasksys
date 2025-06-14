#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <cassert>
#include <utility>
#include <thread>

namespace ts {
  class alignas(64) Job {
#if _DEBUG
    static std::atomic_uint64_t tl_id;
    uint64_t _id;
#endif

    template<typename T>
    using Fn = void __cdecl(T *);

    Fn<void> *_fn;
    void *_data;

  public:
    Job() = default;
    Job(const Job &other) = default;

    template<typename T, typename F>
    Job(F &&fn, T *data)
        : _fn((Fn<void> *) static_cast<Fn<T> *>(fn)),
          _data((void *) data)
#if _DEBUG
        , _id(tl_id.fetch_add(1))
#endif
    {}

    Job &operator=(const Job &other) {
#if _DEBUG
      _id = other._id;
#endif
      _fn = other._fn;
      _data = other._data;
      return *this;
    }

    operator bool() const {
      return _fn;
    }

    void operator()() const {
      if (_fn) {
        _fn(_data);
      }
    }

#if _DEBUG
    bool operator==(const Job &other) const {
      return _id == other._id;
    }

    bool operator!=(const Job &other) const {
      return _id != other._id;
    }
#endif
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