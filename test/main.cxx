
#include <iostream>
#include <print>
#include <x86intrin.h>
#include <oneapi/tbb/task_group.h>

#include "tasksys/Task.h"
#include "tasksys/WorkerGroup.h"

constexpr size_t BOIL_SAMPLE = 16384;
constexpr size_t TEST_SAMPLE = 65536;

unsigned long long rdtsc() {
  unsigned int aux;
  return __rdtscp(&aux);
}

__attribute__((optimize("O0")))
void test_worker_group(int sample) {
  ts::WorkerGroup wg(std::thread::hardware_concurrency());

  const auto begin_time = std::chrono::high_resolution_clock::now();
  const auto begin = rdtsc();

  std::atomic counter = 0;
  for (auto i = 0; i < sample; ++i) {
    wg.push({ [] (auto p) {
      static_cast<std::atomic<int>*>(p)->fetch_add(1, std::memory_order_relaxed);
    }, &counter });
  }
  while (counter.load(std::memory_order_relaxed) < sample) {}

  const auto elapsed = rdtsc() - begin;
  const auto elapsed_time = std::chrono::high_resolution_clock::now() - begin_time;

  std::println("{: >16} {: >8} cycle/op {: >8} ns/op", "ts::WorkerGroup", elapsed / sample, (elapsed_time / sample).count());
  std::flush(std::cout);
}

__attribute__((optimize("O0")))
void test_tbb(int sample) {
  tbb::task_group tg;

  const auto begin_time = std::chrono::high_resolution_clock::now();
  const auto begin = rdtsc();

  std::atomic counter = 0;
  for (auto i = 0; i < sample; ++i) {
    tg.run([&] {
      counter.fetch_add(1, std::memory_order_relaxed);
    });
  }
  while (counter.load(std::memory_order_relaxed) < sample) {}

  const auto elapsed = rdtsc() - begin;
  const auto elapsed_time = std::chrono::high_resolution_clock::now() - begin_time;

  std::println("{: >16} {: >8} cycle/op {: >8} ns/op", "tbb::task_group", elapsed / sample, (elapsed_time / sample).count());
  std::flush(std::cout);
}

ts::Task<int> test_task_inner() {
  std::println("2:{}", std::this_thread::get_id());
  co_return 0;
}

ts::Task<> test_task(ts::WorkerGroup& wg) {
  std::println("1:{}", std::this_thread::get_id());
  co_await test_task_inner().schedule(wg);
  std::println("3:{}", std::this_thread::get_id());
  co_return;
}

int main() {
  test_worker_group(BOIL_SAMPLE);
  test_tbb(BOIL_SAMPLE);
  test_worker_group(TEST_SAMPLE);
  test_tbb(TEST_SAMPLE);

  {
    std::println("0:{}", std::this_thread::get_id());
    ts::WorkerGroup wg(8);
    test_task(wg).schedule(wg).wait();
  }
}
