#include "tasksys/Worker.h"
#include "benchmark.h"
#include <oneapi/tbb/task_group.h>
#include <future>
#include <random>

thread_local std::mt19937 tl_gen(std::random_device{}());
thread_local std::uniform_int_distribution<> tl_distrib(0, 10);

void add_delay() {
  std::this_thread::sleep_for(std::chrono::microseconds(tl_distrib(tl_gen)));
}

BENCHMARK(LocalQueue__latency, ts::LocalQueue) {
  ts::Job job = s_test_jobs[i];

  p->push(job);
  p->pop(&job);
}

BENCHMARK(GlobalQueue__latency, ts::GlobalQueue) {
  ts::Job job = s_test_jobs[i];

  p->push(job);
  p->pop(&job);
}

BENCHMARK(WorkerGroup__latency, ts::WorkerGroup) {
  std::atomic_bool b{false};
  ts::Job job{
      [](void *p) {
        static_cast<std::atomic_bool *>(p)->store(true);
      },
      &b
  };
  p->push(job);

  while (!b) { }
}

BENCHMARK(TaskGroup__latency, tbb::task_group) {
  p->run([] {});
  p->wait();
}

template<size_t N>
void LocalQueue__integrity() {
  static constexpr std::string NAME = "LocalQueue";

  tbb::task_group tg;
  bool failed = false;

  ts::LocalQueue local{ N };

  std::array<ts::Job, N> jobs{};

  // steal before push : to make chaos
  for (auto i = 0; i < N; ++i) {
    tg.run([&, i] {
      add_delay();

      ts::Job job;
      while (!local.steal(&job)) {
        std::this_thread::yield();
      }
      jobs[i] = job;
    });
  }

  for (auto i = 0; i < N; ++i) {
    if (!local.push(s_test_jobs[i])) {
      std::println(std::cerr, "{:>24} : [{}] push failed", NAME, i);
      failed = true;
    }
  }
  if (failed) {
    std::println(std::cerr, "{:>24} : failed", NAME);
    return;
  }

  tg.wait();

  for (auto i = 0; i < N; ++i) {
    bool found = false;
    for (auto j = 0; j < N; ++j) {
      if (s_test_jobs[i] == jobs[j]) {
        found = true;
        break;
      }
    }
    if (!found) {
      std::println(std::cerr, "{:>24} : [{}] item missing", NAME, i);
      failed = true;
    }
  }
  if (failed) {
    std::println(std::cerr, "{:>24} : failed", NAME);
  } else {
    std::println("{:>24} : pass", NAME);
  }
}

template<size_t N>
void GlobalQueue__integrity() {
  static constexpr std::string NAME = "GlobalQueue";

  tbb::task_group tg;

  ts::GlobalQueue local{ N };

  std::array<ts::Job, N> jobs{};
  for (auto i = 0; i < N; ++i) {
    tg.run([&, i] {
      add_delay();

      while (!local.push(s_test_jobs[i])) {
        std::this_thread::yield();
      }
    });
  }
  // tg.wait() <- queue should align itself without external help

  for (auto i = 0; i < N; ++i) {
    tg.run([&, i] {
      add_delay();

      ts::Job job;
      while (!local.pop(&job)) {
        std::this_thread::yield();
      }
      jobs[i] = job;
    });
  }
  tg.wait();

  bool failed = false;
  for (auto i = 0; i < N; ++i) {
    bool found = false;
    for (auto j = 0; j < N; ++j) {
      if (s_test_jobs[i] == jobs[j]) {
        found = true;
        break;
      }
    }
    if (!found) {
      std::println(std::cerr, "{:>24} : [{}] item missing", NAME, i);
      failed = true;
    }
  }
  if (failed) {
    std::println(std::cerr, "{:>24} : failed", NAME);
  } else {
    std::println("{:>24} : pass", NAME);
  }
}

int main() {
  initialize(s_test_jobs);

  std::println("= BENCHMARK");

  ts::LocalQueue local{64};
  BENCHMARK_RUN(LocalQueue__latency, local);

  ts::GlobalQueue global{64};
  BENCHMARK_RUN(GlobalQueue__latency, global);

  ts::WorkerGroup wg{4, 64, 64};
  BENCHMARK_RUN(WorkerGroup__latency, wg);
  wg.stop();

  tbb::task_group tg;
  BENCHMARK_RUN(TaskGroup__latency, tg);

  std::println("= TEST");
  std::flush(std::cout);
  std::flush(std::cerr);

  LocalQueue__integrity<TEST_SAMPLE>();
  GlobalQueue__integrity<TEST_SAMPLE>();
}
