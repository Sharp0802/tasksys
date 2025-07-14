#include "tasksys/ChaseLevDeque.h"

/*
 * Implementation is from:
 *
 * Correct and Efficient Work-Stealing for Weak Memory Models
 *
 * Author: Nhat Minh Lê, Antoniu Pop, Albert Cohen, Francesco Zappa Nardelli
 *    Org: INRIA and ENS Paris
 */

namespace ts {
  std::optional<Job> ChaseLevDeque::pop() {
    // This is the 'take' operation from the paper, executed by the owner thread.
    // It is the only function that decrements `_bottom`.
    // It pops from the 'bottom' (the LIFO end) of the deque.

    // Load bottom and speculatively decrement it to reserve an element.
    // This is a relaxed operation because only the owner thread modifies `_bottom`.
    size_t b = _bottom.load(std::memory_order_relaxed) - 1;
    _bottom.store(b, std::memory_order_relaxed);

    // This fence is crucial. It separates the store to `_bottom` from the load of `_top`.
    // It establishes a single total modification order with the fence in `steal()`
    // and the CAS operations, preventing races when the queue has one element.
    std::atomic_thread_fence(std::memory_order_seq_cst);

    // Load top. A relaxed load is sufficient because the fence above ensures
    // we see a consistent state relative to any stealing threads.
    size_t t = _top.load(std::memory_order_relaxed);

    if (t <= b) {
      // The deque is not empty.
      Job job = _buffer[b & MASK].load(std::memory_order_relaxed);

      if (t == b) {
        // This is the last element in the deque. We must race with stealers.
        // We try to increment `_top` to make the deque empty.
        // The seq_cst ordering here is critical for synchronizing with stealers.
        if (!_top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
          // A stealer won the race. The job is not ours.
          // The deque is now empty, so we must reset `_bottom` to its original state.
          _bottom.store(b + 1, std::memory_order_relaxed);
          return std::nullopt;
        }

        // We won the race. The deque is now empty.
        // We must still reset `_bottom` to be consistent.
        _bottom.store(b + 1, std::memory_order_relaxed);
        return job;
      }

      // There was more than one element, so no race was possible for this specific element.
      // The pop is successful. `_bottom` is left at the new value 'b'.
      return job;
    } else {
      // The deque was empty. Reset `_bottom` to its original value.
      _bottom.store(b + 1, std::memory_order_relaxed);
      return std::nullopt;
    }
  }

  std::optional<Job> ChaseLevDeque::steal() {
    // This operation is executed by a thief thread.
    // It steals from the 'top' (the FIFO end) of the deque.

    // An acquire load on `_top` is needed to synchronize with the owner's push,
    // which does a release store on `_bottom`. This starts the acquire-release chain.
    size_t t = _top.load(std::memory_order_acquire);

    // A seq_cst fence is required to prevent reordering of the read of `_top`
    // and the read of `_bottom`. It synchronizes with the fence in `pop()`.
    std::atomic_thread_fence(std::memory_order_seq_cst);

    // An acquire load on `_bottom` ensures we see the latest value from the owner's push.
    // This pairs with the release store in `push()`.
    size_t b = _bottom.load(std::memory_order_acquire);

    if (t < b) {
      // The deque appears to be non-empty.
      // A relaxed load of the job is sufficient because visibility is guaranteed by the
      // acquire/release synchronization on `_top` and `_bottom`.
      Job job = _buffer[t & MASK].load(std::memory_order_relaxed);

      // Attempt to steal the element by incrementing `_top`.
      // This must be a seq_cst CAS to correctly race against other stealers and the owner's `pop`.
      if (!_top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
        // We lost the race to another stealer or the owner.
        // The pseudo-code returns ABORT; we return nullopt.
        return std::nullopt;
      }

      // Successfully stole the job.
      return job;
    }

    // The deque is empty.
    return std::nullopt;
  }

  bool ChaseLevDeque::push(const Job &job) {
    // This is executed by the owner thread. It pushes to the 'bottom' of the deque.
    size_t b = _bottom.load(std::memory_order_relaxed);
    // An acquire load on `_top` is needed to get the most up-to-date value
    // from stealers, ensuring the size calculation is correct.
    size_t t = _top.load(std::memory_order_acquire);

    if (b - t >= N) {
      // The deque is full.
      return false;
    }

    // Store the job. Relaxed is fine because the subsequent release store on `_bottom`
    // will make this write visible.
    _buffer[b & MASK].store(job, std::memory_order_relaxed);

    // A release store on `_bottom` makes the newly pushed element visible to stealers.
    // Any thread that sees the new value of `_bottom` with an acquire load is
    // guaranteed to see the job stored in the buffer. This must be a store, not a fence.
    _bottom.store(b + 1, std::memory_order_release);

    return true;
  }
}
