#pragma once

#ifndef TS_QUEUE_H
#error "Do not include queue.impl.h directly; Use queue.h instead."
#endif

#include <cassert>
#include <emmintrin.h>

namespace ts {
  using std::memory_order::relaxed;
  using std::memory_order::acquire;
  using std::memory_order::acq_rel;
  using std::memory_order::release;
  using std::memory_order::seq_cst;

  template<typename T>
  queue_item<T>::queue_item(): _p(nullptr) {
  }

  template<typename T>
  queue_item<T>::queue_item(const T *ptr): _p(ptr) {
    assert(ptr != nullptr);
  }

  template<typename T>
  const T & queue_item<T>::read() {
    std::atomic_thread_fence(acquire);
    return *_p;
  }

  template<typename T>
  queue<T>::queue(const size_t size)
    : _queue(size),
      _mask(size - 1),
      _bottom(0),
      _top(0) {
    assert(std::popcount(size) == 1);
  }

  template<typename T>
  std::optional<typename queue<T>::item> queue<T>::push(const T x) {
    if (size() >= _queue.size())
      return std::nullopt;

    const size_t i = _top++ & _mask;
    _queue[i] = x;
    std::atomic_thread_fence(release);

    return item{&_queue[i]};
  }

  template<typename T>
  std::optional<T> queue<T>::pop() {
    if (empty())
      return std::nullopt;
    return _queue[_bottom++ & _mask];
  }


  template<typename T>
  vyukov<T>::vyukov(const size_t size)
    : _buffer(size),
      _mask(size - 1),
      _head(0),
      _tail(0) {
    assert(std::popcount(size) == 1);

    for (size_t i = 0; i < size; ++i) {
      _buffer[i].seq.store(i, relaxed);
    }
    std::atomic_thread_fence(release);
  }

  template<typename T>
  bool vyukov<T>::push(T x) {
    slot* c;

    auto pos = _tail.load(relaxed);
    for (;;) {
      c = &_buffer[pos & _mask];
      const auto seq = c->seq.load(acquire);

      const auto diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
      if (diff == 0) {
        if (_tail.compare_exchange_weak(pos, pos + 1, relaxed))
          break;
      } else if (diff < 0) {
        return false;
      } else {
        pos = _tail.load(relaxed);
      }

      _mm_pause();
    }

    c->data = x;
    c->seq.store(pos + 1, release);
    return true;
  }

  template<typename T>
  std::optional<T> vyukov<T>::pop() {
    slot* c;

    auto pos = _head.load(relaxed);
    for (;;) {
      c = &_buffer[pos & _mask];
      const auto seq = c->seq.load(acquire);

      const auto dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
      if (dif == 0) {
        if (_head.compare_exchange_weak(pos, pos + 1, relaxed))
          break;
      } else if (dif < 0) {
        return std::nullopt;
      } else {
        pos = _head.load(relaxed);
      }

      _mm_pause();
    }

    auto data = c->data;
    c->seq.store(pos + _mask + 1, release);
    return data;
  }


  template<atom T>
  chaselev<T>::chaselev(const size_t size)
    : _buffer(size),
      _mask(size - 1),
      _bottom(0),
      _top(0) {
    assert(std::popcount(size) == 1);
  }

  /*
   * Implementation of chase-lev deque is from below paper:
   *
   * - name: 'Correct and Efficient Work-Stealing for Weak Memory Models'
   * - author: 'Nhat Minh Lê', 'Antoniu Pop', 'Albert Cohen', 'Francesco Zappa Nardelli'
   * - inst: INRIA and ENS Paris
   */

  template<atom T>
  std::optional<T> chaselev<T>::take() {
    const auto bottom = _bottom.load(acquire) - 1;
    _bottom.store(bottom, relaxed);

    std::atomic_thread_fence(seq_cst);

    auto top = _top.load(relaxed);
    if (top > bottom) {
      /* queue is empty; restore */
      _bottom.store(bottom + 1, relaxed);
      return std::nullopt;
    }

    std::optional x = _buffer[bottom & _mask].load(relaxed);

    if (top == bottom) {
      /* race on last item */
      if (!_top.compare_exchange_strong(top, top + 1, seq_cst, relaxed)) {
        /* race failed */
        x = std::nullopt;
      }

      _bottom.store(bottom + 1, relaxed);
    }

    return x;
  }

  template<atom T>
  std::optional<T> chaselev<T>::steal() {
    auto top = _top.load(acquire);
    std::atomic_thread_fence(seq_cst);
    if (top >= _bottom.load(acquire)) {
      return std::nullopt;
    }

    /* non-empty */
    T x = _buffer[top & _mask].load(relaxed);
    if (!_top.compare_exchange_strong(top, top + 1, seq_cst, relaxed))
      /* race failed */
        return std::nullopt;

    return x;
  }

  template<atom T>
  bool chaselev<T>::push(const T x) {
    const size_t bottom = _bottom.load(relaxed);
    const size_t top = _top.load(acquire);

    if (bottom - top > _mask) {
      /* full */
      return false;
    }

    _buffer[bottom & _mask].store(x, relaxed);
    std::atomic_thread_fence(release);
    _bottom.store(bottom + 1, relaxed);

    return true;
  }

  // to ignore IDE inspection
  namespace __ide {
    constexpr int _ = 0;
  }
}
