#include "tasksys/Worker.h"
#include <iostream>

#define BENCHMARK_SAMPLE 10000
#define WARMUP_SAMPLE 100

void task(void *data) {
  volatile bool *p = static_cast<bool *>(data);
  *p = true;
}

[[clang::optnone]]
void LocalQueue__push_pop(void *p) {
  auto *buffer = static_cast<ts::LocalQueue *>(p);

  ts::Job job{ task, nullptr };

  assert(buffer->push(job));
  assert(buffer->pop(&job));
}

[[clang::optnone]]
void GlobalQueue__push_pop(void *p) {
  auto *buffer = static_cast<ts::GlobalQueue *>(p);

  ts::Job job{ task, nullptr };

  buffer->push(job);
  assert(buffer->pop(&job));
}

[[clang::optnone]]
void WorkerGroup__push(void *p) {
  auto *pool = static_cast<ts::WorkerGroup *>(p);

  auto b = false;
  ts::Job job{ task, &b };
  pool->push(job);
  while (!b) {}
}

[[clang::optnone]]
void benchmark(const char *name, void (*fn)(void *), void *p, int sample = BENCHMARK_SAMPLE) {
  auto begin = std::chrono::high_resolution_clock::now();
  for (auto i = 0; i < sample; ++i) {
    fn(p);
  }
  std::chrono::nanoseconds elapsed = std::chrono::high_resolution_clock::now() - begin;
  if (name != nullptr) {
    std::cout << name << "\t: " << (elapsed.count() / sample) << "ns\n";
  }
}

int main() {
  ts::LocalQueue local{64 };
  benchmark(nullptr, LocalQueue__push_pop, &local, WARMUP_SAMPLE);
  benchmark("local-queue", LocalQueue__push_pop, &local);

  ts::GlobalQueue global;
  benchmark(nullptr, GlobalQueue__push_pop, &global, WARMUP_SAMPLE);
  benchmark("global-queue", GlobalQueue__push_pop, &global);

  ts::WorkerGroup pool{ 4, 64 };
  benchmark(nullptr, WorkerGroup__push, &pool, WARMUP_SAMPLE);
  benchmark("thread-pool", WorkerGroup__push, &pool);
  pool.stop();
}
