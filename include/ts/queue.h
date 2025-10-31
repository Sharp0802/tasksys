#pragma once

#define TS_QUEUE_H

#include <atomic>
#include <optional>
#include <vector>

namespace ts {
  template<typename T>
  class queue_item {
    const T *_p;

  public:
    queue_item();
    explicit queue_item(const T *ptr);

    const T &read();
  };

  /**
   * Bounded queue implementation
   * for single-threaded manipulation and multithreaded viewing.
   */
  template<typename T>
  class queue {
    std::vector<T> _queue;
    size_t _mask;
    size_t _bottom;
    size_t _top;

  public:
    using item = queue_item<T>;

    /**
     * Creates new queue.
     *
     * @param size Capacity of queue. Must be power of 2.
     */
    explicit queue(size_t size);

    [[nodiscard]] size_t size() const { return _top - _bottom; }
    [[nodiscard]] bool empty() const { return _bottom == _top; }

    std::optional<item> push(T x);
    std::optional<T> pop();
  };

  constexpr size_t CACHELINE_SIZE = std::hardware_destructive_interference_size;

  template<typename T>
  concept atom = std::is_trivially_copyable_v<T> && std::atomic<T>::is_always_lock_free;

  /**
   * fetch-and-add queue implementation
   */
  template<atom T>
  class faa {
    std::vector<T> _queue;
    size_t _mask;

    alignas(CACHELINE_SIZE) std::atomic_size_t _head;
    alignas(CACHELINE_SIZE) std::atomic_size_t _prepared;
    alignas(CACHELINE_SIZE) std::atomic_size_t _tail;

  public:
    using item = queue_item<T>;

    explicit faa(size_t size);
    std::optional<item> push(T x);
    std::optional<item> pop();
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
