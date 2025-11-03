#pragma once

#define TS_QUEUE_H

#include <atomic>
#include <memory>
#include <optional>
#include <vector>

namespace ts {
  /**
   * Unbounded ring queue
   */
  template<typename T>
  class queue {
    std::unique_ptr<T[]> _buffer;
    size_t _mask;
    size_t _tail;
    size_t _head;

    void resize(size_t size);

  public:
    queue();

    [[nodiscard]] size_t size() const { return _head - _tail; }
    [[nodiscard]] bool empty() const { return _tail == _head; }

    void push(T x);
    std::optional<T> pop();
  };

  /**
   * Object pool implementation
   * for single-thread
   * based on unbounded ring queue
   */
  template<typename T>
  class pool {
    queue<T*> _queue;

  public:
    pool();
    ~pool();

    template<typename... Args>
    T *rent(Args &&... args);

    void yield(T *p);
  };

  /**
   * Object pool implementation
   * for multi-thread
   * based on thread-local object pool
   */
  template<typename T>
  class mt_pool {
    static pool<T> *inner() {
      thread_local pool<T> inner;
      return &inner;
    }

  public:
    template<typename... Args>
    static T *rent(Args &&... args) {
      return inner()->rent(std::forward<Args>(args)...);
    }

    static void yield(T *p) {
      inner()->yield(p);
    }
  };

  constexpr size_t CACHELINE_SIZE = std::hardware_destructive_interference_size;

  template<typename T>
  concept atom = std::is_trivially_copy_constructible_v<T> && std::atomic<T>::is_always_lock_free;

  /**
   * Dmitry Vyukov's mpmc queue implementation
   */
  template<typename T>
  class vyukov {
    struct slot {
      std::atomic_size_t seq;
      T data;
    };

    std::vector<slot> _buffer;
    size_t _mask;

    std::atomic_size_t _available;
    std::atomic_flag _alive = ATOMIC_FLAG_INIT;

    alignas(CACHELINE_SIZE) std::atomic_size_t _head;
    alignas(CACHELINE_SIZE) std::atomic_size_t _tail;

  public:
    explicit vyukov(size_t size);

    bool push(T x);
    bool blocking_push(T x);
    std::optional<T> pop();
    std::optional<T> blocking_pop();

    void kill();

    // note: ignores inner items; may leak
    void unsafe_reset();
  };

  /**
   * Bounded chase-lev deque implementation.
   */
  template<atom T>
  class chaselev {
    std::vector<std::atomic<T>> _buffer;
    size_t _mask;

    alignas(CACHELINE_SIZE) std::atomic_size_t _bottom;
    alignas(CACHELINE_SIZE) std::atomic_size_t _top;

  public:
    /**
     * Creates new deque.
     *
     * @param size Capacity of deque. Must be power of 2.
     */
    explicit chaselev(size_t size);

    std::optional<T> take();
    std::optional<T> steal();
    bool push(T x);
  };
}

#include "queue.impl.h"

// to ignore ide inspection
namespace ts::__ide {
  constexpr auto __ide = _;
}

#undef TS_QUEUE_H
