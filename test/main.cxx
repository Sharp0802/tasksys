#include "tasksys/tasksys.h"
#include "tasksys/workqueue.h"
#include <iostream>

ts::Task<int> compute() {
  co_return 100;
}

void task(void *data) {
  volatile bool *p = static_cast<bool *>(data);
  *p = true;
}

__attribute__((optimize("O0")))
void tp_benchmark(ts::ThreadPool& tp, long *t) {
  volatile bool p;

  auto begin = std::chrono::high_resolution_clock::now();
  tp.push(task, (void *) &p);
  while (!p) {}
  std::chrono::nanoseconds elapsed = std::chrono::high_resolution_clock::now() - begin;

  *t += duration_cast<std::chrono::nanoseconds>(elapsed).count();
}

int main() {

  ts::ThreadPool tp{2, 128};

  long t = 0;
  tp_benchmark(tp, &t); // warm-up
  t = 0;

  for (int i = 0; i < 1000000; ++i) {
    tp_benchmark(tp, &t);
  }

  std::cout << (t / 1000000) << std::flush;
  tp.stop();
}
