#pragma once

#include <atomic>

#include "Assert.h"

namespace ts {
  class Job {
    void (*_fn)(void *);
    void *_data;

  public:
    Job() : _fn(nullptr), _data(nullptr) {}
    Job(const decltype(_fn) fn, void *data) : _fn(fn), _data(data) {}

    [[nodiscard]] decltype(_fn) fn() const { return _fn; }
    [[nodiscard]] const void *data() const { return _data; }

    void operator()() const {
      Assert(_fn && "_fn cannot be null");
      _fn(_data);
    }
  };

  static_assert(sizeof(Job) == 2 * sizeof(void *));

  static_assert(__atomic_always_lock_free(sizeof(Job), nullptr));

#ifndef TS_USE_CUSTOM_JOBSLOT
  using JobSlot = std::atomic<Job>;
#else
  static_assert(static_cast<int>(std::memory_order_relaxed) == __ATOMIC_RELAXED);
  static_assert(static_cast<int>(std::memory_order_consume) == __ATOMIC_CONSUME);
  static_assert(static_cast<int>(std::memory_order_acquire) == __ATOMIC_ACQUIRE);
  static_assert(static_cast<int>(std::memory_order_release) == __ATOMIC_RELEASE);
  static_assert(static_cast<int>(std::memory_order_acq_rel) == __ATOMIC_ACQ_REL);
  static_assert(static_cast<int>(std::memory_order_seq_cst) == __ATOMIC_SEQ_CST);

  class JobSlot {
#if UINTPTR_WIDTH == 64
    using double_intptr = __int128_t;
#elif UINTPTR_WIDTH == 32
    using double_intptr = __int64_t;
#else
#error Only 64/32 bits machines are supported
#endif

#ifndef __GNUC__
#error Compiler may not support type punning on union; Use GCC or Clang instead
#endif
    // NOLINTNEXTLINE(*-pro-type-member-init)
    volatile union {
      Job _job;
      double_intptr _value;
    };

    explicit JobSlot(const double_intptr value) : _value(value) {}

  public:
    JobSlot() : _value() {}

    void store(const Job job, std::memory_order order) {
      const struct _Union {
        // NOLINTNEXTLINE(*-pro-type-member-init)
        union {
          Job job;
          double_intptr value;
        };

        explicit _Union(const Job job) : job(job) {}
      } _union = _Union{ job };

      __atomic_store_n(&this->_value, _union.value, static_cast<int>(order));
    }

    [[nodiscard]] Job load(std::memory_order order) const {
      return JobSlot{__atomic_load_n(&this->_value, static_cast<int>(order))}._job;
    }
  };
#endif
}
