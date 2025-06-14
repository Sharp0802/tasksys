#include "tasksys/GlobalQueue.h"
#include "tasksys/memory.h"
#include <vector>
#include <execution>

ts::GlobalQueue::GlobalQueue(size_t size) : _mask(size - 1) {
  assert(std::popcount(size) == 1);

  _array = ts::alloc<Job>(size);
}

ts::GlobalQueue::~GlobalQueue() {
  ts::free(_array);
}

bool ts::GlobalQueue::push(const ts::Job &job) {
  auto head = _head.fetch_add(1, std::memory_order_acq_rel);

  if (head - _tail.load(std::memory_order_acquire) >= (_mask + 1)) {
    return false;
  }

  _array[head & _mask] = job;

  auto expected_prepared = head;
  while (!_prepared.compare_exchange_weak(
      expected_prepared, head + 1,
      std::memory_order_release,
      std::memory_order_relaxed)) {
    expected_prepared = head;
    _mm_pause();
  }

  return true;
}

bool ts::GlobalQueue::pop(ts::Job *job) {
  auto tail = _tail.load(std::memory_order_relaxed);
  while (true) {
    if (tail >= _prepared.load(std::memory_order_acquire)) {
      return false;
    }

    if (_tail.compare_exchange_weak(
        tail, tail + 1,
        std::memory_order_acq_rel,
        std::memory_order_relaxed)) {
      *job = _array[tail & _mask];
      return true;
    }

    _mm_pause();
  }
}
