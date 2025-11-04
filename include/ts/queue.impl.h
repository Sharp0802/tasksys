#pragma once

#ifndef TS_QUEUE_H
#error "Do not include queue.impl.h directly; Use queue.h instead."
#endif

#include <cassert>
#if defined(__i386__) || defined(__x86_64__)
#include <emmintrin.h>
#endif

namespace ts {
  using std::memory_order::relaxed;
  using std::memory_order::acquire;
  using std::memory_order::acq_rel;
  using std::memory_order::release;
  using std::memory_order::seq_cst;

  inline void may_relax() noexcept {
#if defined(_MSC_VER) || __clang__
    _mm_pause();
#elif defined(__i386__) || defined(__x86_64__)
    __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
    __asm__ __volatile__("yield" ::: "memory");
#endif
  }

  template<typename T>
  void queue<T>::resize(size_t size) {
    assert(std::popcount(size) == 1);
    assert(size > _mask + 1);

    auto new_v = std::make_unique<T[]>(size);
    for (size_t i = 0; i < _head - _tail; ++i) {
      new_v[i] = std::move_if_noexcept(_buffer[(_tail + i) & _mask]);
    }

    _buffer.swap(new_v);
    _mask = size - 1;
    _head -= _tail;
    _tail = 0;
  }

  template<typename T>
  queue<T>::queue()
    : _buffer(std::make_unique<T[]>(16)),
      _mask(15),
      _tail(0),
      _head(0) {
  }

  template<typename T>
  void queue<T>::push(T x) {
    if (_head - _tail > _mask) {
      resize((_mask + 1) * 2);
    }

    _buffer[_head++ & _mask] = std::move_if_noexcept(x);
  }

  template<typename T>
  std::optional<T> queue<T>::pop() {
    if (empty()) {
      return std::nullopt;
    }

    const auto i = _tail++ & _mask;
#if NDEBUG
    return std::move_if_noexcept(_buffer[i]);
#else
    T x{};
    std::swap(x, _buffer[i]);
    return std::move_if_noexcept(x);
#endif
  }

  template<typename T>
  pool<T>::pool() {
  }

  template<typename T>
  pool<T>::~pool() {
    while (true) {
      if (auto p = _queue.pop()) {
        ::operator delete(p.value());
        continue;
      }

      break;
    }
  }

  template<typename T>
  template<typename... Args>
  T *pool<T>::rent(Args &&... args) {
    if (auto v = _queue.pop()) {
      auto p = v.value();
      try {
        new(p) T(std::forward<Args>(args)...);
        return p;
      }
      catch (...) {
        delete p;
        throw;
      }
    }

    return new T(std::forward<Args>(args)...);
  }

  template<typename T>
  void pool<T>::yield(T *p) {
    assert(p != nullptr);

    p->~T();
    try {
      _queue.push(p);
    }
    catch (...) {
      delete p;
      throw;
    }
  }

  template<typename T>
  vyukov<T>::vyukov(const size_t size)
    : _buffer(size),
      _mask(size - 1),
      _available(0),
      _head(0),
      _tail(0) {
    assert(std::popcount(size) == 1);

    _alive.test_and_set(relaxed);
    for (size_t i = 0; i < size; ++i) {
      _buffer[i].seq.store(i, relaxed);
    }

    std::atomic_thread_fence(release);
  }

  template<typename T>
  bool vyukov<T>::push(T x) {
    slot *c;

    auto pos = _tail.load(relaxed);
    for (;;) {
      c = &_buffer[pos & _mask];
      const auto seq = c->seq.load(acquire);

      const auto diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
      if (diff == 0) {
        if (_tail.compare_exchange_weak(pos, pos + 1, relaxed))
          break;
      }
      else if (diff < 0) {
        return false;
      }
      else {
        pos = _tail.load(relaxed);
      }

      may_relax();
    }

    c->data = x;
    c->seq.store(pos + 1, release);

    _available.fetch_add(1, release);
    _available.notify_one();
    return true;
  }

  template<typename T>
  bool vyukov<T>::blocking_push(T x) {
    bool alive;
    do {
      _available.wait(_mask + 1, acquire);
      alive = _alive.test(acquire);
    } while (alive && !push(std::move(x)));

    return alive;
  }

  template<typename T>
  std::optional<T> vyukov<T>::pop() {
    slot *c;

    auto pos = _head.load(relaxed);
    for (;;) {
      c = &_buffer[pos & _mask];
      const auto seq = c->seq.load(acquire);

      const auto dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
      if (dif == 0) {
        if (_head.compare_exchange_weak(pos, pos + 1, relaxed))
          break;
      }
      else if (dif < 0) {
        return std::nullopt;
      }
      else {
        pos = _head.load(relaxed);
      }

      may_relax();
    }

    auto data = std::move_if_noexcept(c->data);
    c->seq.store(pos + _mask + 1, release);

    _available.fetch_sub(1, acq_rel);
    _available.notify_one();
    return std::move_if_noexcept(data);
  }

  template<typename T>
  std::optional<T> vyukov<T>::blocking_pop() {
    for (;;) {
      bool alive;
      do {
        _available.wait(0, acquire);
        alive = _alive.test(acquire);
      } while (_available.load(acquire) <= 0 && alive);

      if (auto v = pop())
        return v;

      if (!alive) {
        return std::nullopt;
      }
    }
  }

  template<typename T>
  void vyukov<T>::kill() {
    _alive.clear(relaxed);
    /*
     * It's "fake not-empty".
     *
     * Without sentinel value, `blocking_pop` can be in deadlock
     * when `_available` is zero and `wait` is called after `notify_all`.
     *
     * So, non-zero state of `_available` is required
     * to avoid another "state" variable to remove deadlock.
     */
    _available.store(std::numeric_limits<size_t>::max() / 2 /* sentinel */, release);
    _available.notify_all();
  }

  template<typename T>
  void vyukov<T>::unsafe_reset() {
    _available.store(0, release);
    _head.store(0, release);
    _tail.store(0, release);
  }


  template<typename T>
  buffer_desc<T>::buffer_desc(const size_t size): _data(new std::atomic<T>[size]), _size(size), _mask(size - 1) {
    assert(std::popcount(size) == 1);
  }

  template<typename T>
  buffer_desc<T>::~buffer_desc() {
    delete[] _data;
  }

  template<typename T>
  size_t buffer_desc<T>::size() const {
    return _size;
  }

  template<typename T>
  T buffer_desc<T>::load(const size_t i, std::memory_order order) {
    return _data[i & _mask].load(order);
  }

  template<typename T>
  void buffer_desc<T>::store(const size_t i, T x, std::memory_order order) {
    _data[i & _mask].store(x, order);
  }


  template<typename T>
  buffer<T>::buffer(const size_t size) : _inner(new buffer_desc<T>(size)), _past(nullptr) {
    assert(std::popcount(size) == 1);
  }

  template<typename T>
  buffer<T>::~buffer() {
    delete _inner.load(acquire);
    delete _past;
  }

  template<typename T>
  void buffer<T>::resize(const size_t begin, const size_t end) {
    const auto inner = _inner.load(relaxed);
    const auto new_size = inner->size() * 2;

    auto new_inner = new buffer_desc<T>(new_size);
    for (size_t i = begin; i < end; i++) {
      new_inner->store(i % new_size, inner->load(i % inner->size(), relaxed), relaxed);
    }

    delete _past;
    _past = _inner.exchange(new_inner, release);
  }


  template<atom T>
  chaselev<T>::chaselev(const size_t size)
    : _buffer(size),
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
    const auto array = _buffer.get(relaxed);
    _bottom.store(bottom, relaxed);

    std::atomic_thread_fence(seq_cst);

    auto top = _top.load(relaxed);
    if (top > bottom) {
      /* queue is empty; restore */
      _bottom.store(bottom + 1, relaxed);
      return std::nullopt;
    }

    std::optional x(array->load(bottom, relaxed));

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
    auto array = _buffer.get(relaxed);
    T x(array->load(top, relaxed));
    if (!_top.compare_exchange_strong(top, top + 1, seq_cst, relaxed))
      /* race failed */
      return std::nullopt;

    return x;
  }

  template<atom T>
  void chaselev<T>::push(const T x) {
    const auto bottom = _bottom.load(relaxed);
    const auto top = _top.load(acquire);

    auto array = _buffer.get(relaxed);
    if (bottom - top >= array->size()) {
      _buffer.resize(top, bottom);
      array = _buffer.get(relaxed);
    }

    array->store(bottom, x, relaxed);
    std::atomic_thread_fence(release);
    _bottom.store(bottom + 1, relaxed);
  }

  template<atom T>
  bool chaselev<T>::try_push(T x) {
    const auto bottom = _bottom.load(relaxed);
    const auto top = _top.load(acquire);

    auto array = _buffer.get(relaxed);
    if (bottom - top >= array->size()) {
      return false;
    }

    array->store(bottom, x, relaxed);
    std::atomic_thread_fence(release);
    _bottom.store(bottom + 1, relaxed);

    return true;
  }

  // to ignore IDE inspection
  namespace __ide {
    constexpr int _ = 0;
  }
}
