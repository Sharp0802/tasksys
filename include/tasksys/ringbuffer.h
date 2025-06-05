#pragma once

#include <atomic>
#include <cassert>
#include <memory>
#include <optional>

namespace ts {
  template<typename T>
  class RingBuffer {
    alignas(64) std::atomic_size_t _head = 0;
    alignas(64) std::atomic_size_t _prepared = 0;
    alignas(64) std::atomic_size_t _tail = 0;
    T *_buffer;
    size_t _size;
    size_t _mask;

  public:
    RingBuffer(const RingBuffer &) = delete;
    RingBuffer &operator=(const RingBuffer &) = delete;

    explicit RingBuffer(size_t size) : _size(size) {
      assert(size > 2 && "ring-buffer reserved must be greater than 2");
      assert(std::popcount(size) == 1 && "ring-buffer reserved must be power of 2");

      _mask = size - 1;
      _buffer = static_cast<T *>(operator new[](sizeof(T) * _size, std::align_val_t{alignof(T)}));
    }

    ~RingBuffer() {
      auto tail = _tail.load(std::memory_order_relaxed);
      auto head = _head.load(std::memory_order_relaxed);

      while (tail != head) {
        std::launder(_buffer + tail)->~T();
        tail = (tail + 1) & _mask;
      }

      operator delete[]((void *) _buffer, std::align_val_t{alignof(T)});
    }

    [[nodiscard]]
    size_t reserved() const noexcept {
      size_t tail = _tail.load(std::memory_order_acquire);
      size_t head = _head.load(std::memory_order_acquire);

      if (head >= tail) {
        return head - tail;
      } else {
        return _size - (tail - head);
      }
    }

    [[nodiscard]]
    size_t prepared() const noexcept {
      size_t tail = _tail.load(std::memory_order_acquire);
      size_t prepared = _prepared.load(std::memory_order_acquire);

      if (prepared >= tail) {
        return prepared - tail;
      } else {
        return _size - (tail - prepared);
      }
    }

    bool push(T &&item) noexcept(std::is_nothrow_move_constructible_v<T>) {
      size_t next, head = _head.load(std::memory_order_relaxed);
      while (true) {
        next = (head + 1) & _mask;

        auto tail = _tail.load(std::memory_order_relaxed);
        if (next == tail) {
          return false;
        }

        if (_head.compare_exchange_weak(head, next, std::memory_order_acq_rel, std::memory_order_relaxed)) {
          break;
        }
      }

      std::construct_at(std::launder(_buffer + head), std::move(item));

      size_t head_c;
      while (true) {
        head_c = head;
        if (_prepared.compare_exchange_weak(head_c, next, std::memory_order_acq_rel, std::memory_order_relaxed)) {
          break;
        }
      }

      return true;
    }

    std::optional<T> pop() noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_destructible_v<T>) {
      auto tail = _tail.load(std::memory_order_relaxed);
      auto prepared = _prepared.load(std::memory_order_acquire);
      if (prepared == tail) {
        return std::nullopt;
      }

      T *p = std::launder(_buffer + tail);
      T v = std::move(*p);
      p->~T();

      tail = (tail + 1) & _mask;
      _tail.store(tail, std::memory_order_release);

      return v;
    }
  };
}
