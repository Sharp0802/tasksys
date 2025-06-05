#pragma once

#include <utility>
#include <thread>
#include <execution>

#include "ringbuffer.h"

namespace ts {
  class WorkQueue {
    using Handle = std::pair<void (*)(void *), void *>;

    static constexpr int MAX_SPIN = 1000;

    RingBuffer<Handle> _queue;
    std::counting_semaphore<> _available;
    std::counting_semaphore<> _free;
    std::jthread _thread;
    std::stop_token _token;

    static void loop(WorkQueue *self) noexcept {
      while (!self->_token.stop_requested()) {
        for (auto spins = 0;
             spins < MAX_SPIN &&
             self->_queue.prepared() == 0 &&
             !self->_token.stop_requested();
             ++spins) {
#if __x86_64__
          _mm_pause(); // spin-lock hint to cpu
#endif
        }

        if (self->_queue.prepared() == 0) {
          self->_available.acquire();
        }
        while (self->_queue.prepared() == 0 && !self->_token.stop_requested()) {
          // `available > 0` but `prepared == 0` is possible (spuriously)
          std::this_thread::yield();
        }
        if (self->_token.stop_requested()) {
          break;
        }

        auto v = self->_queue.pop();
        assert(v.has_value() && "pop() failed despite semaphore count");
        auto [fn, data] = v.value();
        fn(data);

        self->_free.release();
      }
    }

  public:
    explicit WorkQueue(size_t queue_size)
        : _queue(queue_size),
          _available(0),
          _free((ptrdiff_t) (queue_size - 1)),
          _thread(loop, this),
          _token(_thread.get_stop_token()) {
      assert((queue_size - 1) < std::numeric_limits<ptrdiff_t>::max());
    }

    ~WorkQueue() {
      stop();
    }

    [[nodiscard]]
    size_t pressure() const {
      return _queue.reserved() + _queue.prepared();
    }

    void stop() {
      if (!_thread.joinable()) {
        return;
      }

      _thread.request_stop();
      _available.release();
      _thread.join();
    }

    void push(void(*fn)(void *), void *data) noexcept {
      _free.acquire();

      bool ok = _queue.push(std::make_pair(fn, data));
      assert(ok && "push() failed despite semaphore count");

      _available.release();
    }

    bool try_push(void(*fn)(void *), void *data) noexcept {
      if (!_free.try_acquire()) {
        return false;
      }

      bool ok = _queue.push(std::make_pair(fn, data));
      assert(ok && "push() failed despite semaphore count");

      _available.release();
      return true;
    }
  };
}
