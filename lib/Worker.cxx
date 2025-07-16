#include "tasksys/Worker.h"

#include "tasksys/WorkerGroup.h"

#if defined(__i386__) || defined(__x86_64__)
#include <x86intrin.h>
#define TS_IS_X86 1
#endif

namespace {
  void relax() {
#if TS_IS_X86
    _mm_pause();
#else
    std::this_thread::yield();
#endif
  }
}

namespace ts {
  void Worker::run_job(const Job job) const {
    WorkerGroup::set_current(_wg);
    job();
  }

  ChaseLevDeque &Worker::steal_target() const {
    static size_t _steal_id = _id;
    do {
      _steal_id = (_steal_id + 1) % _queues.size();
    }
    while (_steal_id == _id);
    return _queues[_steal_id];
  }

  bool Worker::steal_one() const {
    for (auto i = 0; i < STEAL_LIMIT; ++i) {
      for (auto j = 0; j < _queues.size() - 1; ++j) {
        if (const auto job = steal_target().steal()) {
          run_job(job.value());
          return true;
        }
      }

      relax();
    }

    return false;
  }

  bool Worker::process_one() const {
    for (auto i = 0; i < LOCAL_LIMIT; ++i) {
      if (const auto job = _queues[_id].pop()) {
        run_job(job.value());
        return true;
      }

      relax();
    }

    return false;
  }

  bool Worker::process_global_one() const {
    for (auto i = 0; i < GLOBAL_LIMIT; ++i) {
      if (const auto job = _global.pop()) {
        run_job(job.value());
        return true;
      }

      relax();
    }

    return false;
  }

  void Worker::run(const std::stop_token &token) const {
    using namespace std::chrono_literals;

    while (!token.stop_requested()) {
      if (process_one()) continue;
      if (process_global_one()) continue;
      if (steal_one()) continue;

      using namespace std::chrono_literals;
      std::this_thread::sleep_for(100ns);
    }
  }

  Worker::Worker(const size_t id, WorkerGroup &wg, std::vector<ChaseLevDeque> &queues, FAAQueue &global)
    : _id(id),
      _queues(queues),
      _global(global),
      _wg(wg),
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

      relax();
    }
  }

  std::thread::id Worker::thread_id() const {
    return _thread.get_id();
  }
}
