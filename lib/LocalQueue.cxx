#include <random>
#include "tasksys/LocalQueue.h"

ts::LocalQueue::LocalQueue(size_t size) : _mask(size - 1), _head(0), _tail(0) {
  assert(std::popcount(size) == 1);

  _array = std::make_unique<Job[]>(size);
}

bool ts::LocalQueue::pop(Job *job) noexcept {
  size_t tail = _tail.load(std::memory_order_relaxed) - 1;
  _tail.store(tail, std::memory_order_relaxed);

  size_t head = _head.load(std::memory_order_acquire);
  if (head <= tail) {
    *job = _array[tail & _mask];
    return true;
  }

  _tail.store(tail + 1, std::memory_order_relaxed);
  return false;
}

bool ts::LocalQueue::steal(Job *job) noexcept {
  size_t head = _head.load(std::memory_order_acquire);
  size_t tail = _tail.load(std::memory_order_acquire);
  if (head >= tail) {
    return false;
  }

  __builtin_prefetch(&_array[(head + 1) & _mask], 0, 1);

  *job = _array[head & _mask];
  if (!_head.compare_exchange_strong(
      head, head + 1,
      std::memory_order_acq_rel,
      std::memory_order_relaxed)) {
    return false;
  }
  return true;
}

bool ts::LocalQueue::push(const Job &job) noexcept {
  size_t tail = _tail.load(std::memory_order_relaxed);
  size_t head = _head.load(std::memory_order_acquire);
  if ((tail - head) > _mask) {
    return false;
  }

  _array[tail & _mask] = job;
  _tail.store(tail + 1, std::memory_order_relaxed);
  return true;
}
