#include "tasksys/Worker.h"

#if defined(__i386__) || defined(__x86_64__)
#include <x86intrin.h>
#define TS_IS_X86 1
#endif

namespace ts {
  bool Worker::process_one() const {
    if (const auto job = _queues[_id].pop()) {
      job.value()();
      return true;
    }

    return false;
  }

  ChaseLevDeque &Worker::steal_target() {
    do {
      ++_steal_id % _queues.size();
    }
    while (_steal_id == _id);
    return _queues[_steal_id];
  }

  bool Worker::steal_one() {
    for (auto i = 0; i < _queues.size() - 1; ++i) {
      if (const auto job = steal_target().steal()) {
        job.value()();
        return true;
      }
    }

    return false;
  }

  void Worker::run(const std::stop_token &token) {
    using namespace std::chrono_literals;

    while (!token.stop_requested()) {
      auto did = false;
      for (auto i = 0; i < SPIN_LIMIT; ++i) {
        if (process_one()) {
          did = true;
          break;
        }

#if TS_IS_X86
        _mm_pause();
#else
          std::this_thread::yield();
#endif
      }
      if (did) {
        continue;
      }

      for (auto i = 0; i < STEAL_LIMIT; ++i) {
        if (steal_one()) {
          did = true;
          break;
        }

        std::this_thread::yield();
      }
      if (did) {
        continue;
      }

      using namespace std::chrono_literals;
      std::this_thread::sleep_for(1ms);
    }
  }

  Worker::Worker(const size_t id, std::vector<ChaseLevDeque> &queues)
    : _id(id),
      _steal_id(id),
      _queues(queues),
      _thread(&Worker::run, this, _ss.get_token()) {
  }

  Worker::~Worker() {
    stop();
  }

  // ReSharper disable once CppMemberFunctionMayBeConst
  void Worker::stop() {
    if (!_ss.stop_possible())
      return;

    [[maybe_unused]] auto _ = _ss.request_stop(); // request_stop only fails when stop_possible is false
  }

  void Worker::push(const Job &job) const {
    while (!_queues[_id].push(job)) {
      if (const auto old = _queues[_id].pop()) {
        old.value()();
        continue;
      }

#if TS_IS_X86
      _mm_pause();
#else
        std::this_thread::yield();
#endif
    }
  }

  std::thread::id Worker::thread_id() const {
    return _thread.get_id();
  }
}
