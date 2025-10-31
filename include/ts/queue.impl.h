#pragma once

#include <cassert>

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


  template<atom T>
  faa<T>::faa(const size_t size)
    : _queue(size),
      _mask(size - 1),
      _head(0),
      _prepared(0),
      _tail(0) {
    assert(std::popcount(size) == 1);
  }

  template<atom T>
  std::optional<typename faa<T>::item> faa<T>::push(T x) {
    auto head = _head.load(acquire);
    do {
      if (head - _tail.load(acquire) >= _queue.size()) {
        return std::nullopt;
      }
    } while (!_head.compare_exchange_weak(head, head + 1, acq_rel, acquire));

    const auto i = head & _mask;
    _queue[i] = x;

    decltype(head) prepared;
    do {
      prepared = head;
    } while (!_prepared.compare_exchange_weak(prepared, prepared + 1, acq_rel, acquire));

    return item{&_queue[i]};
  }

  template<atom T>
  std::optional<typename faa<T>::item> faa<T>::pop() {
    std::optional<item> x;

    auto tail = _tail.load(acquire);
    do {
      if (tail == _prepared.load(acquire))
        return std::nullopt;
      x = item{&_queue[tail & _mask]};
    } while (!_tail.compare_exchange_weak(tail, tail + 1, acq_rel, acquire));

    return x;
  }


  template<atom T>
  chaselev<T>::chaselev(const size_t size)
    : _buffer(size),
      _mask(size - 1),
      _bottom(0),
      _top(0) {
    assert(std::popcount(size) == 1);
  }

  template<atom T>
  std::optional<T> chaselev<T>::take() {
    const auto bottom = _bottom.load(acquire) - 1;
    _bottom.store(bottom, relaxed);

    auto top = _top.load(acquire);
    if (top > bottom) {
      /* queue is empty; restore */
      _bottom.store(bottom + 1, relaxed);
      return std::nullopt;
    }

    std::optional x = _buffer[bottom & _mask].load(acquire);

    if (top == bottom) {
      /* race on last item */
      if (!_top.compare_exchange_strong(top, top + 1, acq_rel, relaxed)) {
        /* race failed */
        x = std::nullopt;
      }

      _bottom.store(bottom + 1, release);
    }

    return x;
  }

  template<atom T>
  std::optional<T> chaselev<T>::steal() {
    auto top = _top.load(acquire);
    if (top >= _bottom.load(acquire)) {
      return std::nullopt;
    }

    /* non-empty */
    T x = _buffer[top & _mask].load(relaxed);
    if (!_top.compare_exchange_strong(top, top + 1, acq_rel, relaxed))
      /* race failed */
        return std::nullopt;

    return x;
  }

  template<atom T>
  bool chaselev<T>::push(const T x) {
    const size_t bottom = _bottom.load(acquire);
    const size_t top = _top.load(acquire);

    if (bottom - top > _mask) {
      /* full */
      return false;
    }

    _buffer[bottom & _mask].store(x, relaxed);
    _bottom.store(bottom + 1, release);

    return true;
  }

  // to ignore IDE inspection
  namespace __ide {
    constexpr int _ = 0;
  }
}
