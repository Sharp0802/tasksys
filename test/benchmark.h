#pragma once

#include <cstdint>
#include <x86intrin.h>
#include <print>
#include <iostream>
#include <oneapi/tbb/task_group.h>
#include "tasksys/LocalQueue.h"

#define COMPILER_FENCE() asm volatile("" ::: "memory")
#define NOINLINE __attribute__((noinline))

#define BENCHMARK_SAMPLE 10000
#define TEST_SAMPLE 131072

#if TEST_SAMPLE > BENCHMARK_SAMPLE
#define MAX_SAMPLE TEST_SAMPLE
#else
#define MAX_SAMPLE BENCHMARK_SAMPLE
#endif

#define WARMUP_SAMPLE 100

NOINLINE uint64_t rdtsc() {
  unsigned aux;
  return __rdtscp(&aux);
}

[[clang::optnone]]
NOINLINE void benchmark_internal(const char *name, void (*fn)(int, void *), void *p, int sample) {
  for (auto i = 0; i < WARMUP_SAMPLE; ++i) {
    fn(i, p);
    COMPILER_FENCE();
  }

  auto begin = rdtsc();
  for (auto i = 0; i < sample; ++i) {
    fn(i, p);
    COMPILER_FENCE();
  }
  auto end = rdtsc();
  auto avg_cycles = double(end - begin) / sample;

  std::println("{:>24} : {:.2f} cycles", name, avg_cycles);
}

template<typename T>
inline void benchmark(const char *name, void (*fn)(int, T *), T *p, int sample = BENCHMARK_SAMPLE) {
  benchmark_internal(name, reinterpret_cast<void (*)(int, void *)>(fn), p, sample);
}

[[clang::optnone]]
NOINLINE void test_internal(const char *name, const char* (*fn)(int, void *), void *p) {

  tbb::task_group tg;

  std::atomic_bool succeed = true;
  for (auto i = 0; i < BENCHMARK_SAMPLE; ++i) {
    tg.run([&] {
      if (auto msg = fn(i, p); msg) {
        std::println(std::cerr, "{:>24} : {}", name, msg);
        succeed = false;
      }
    });
  }
  tg.wait();
  if (succeed) {
    std::println("{:>24} : pass", name);
  } else {
    std::println(std::cerr, "{:>24} : failed", name);
  }
}

template<typename T>
inline void test(const char *name, const char* (*fn)(int, T *), T *p) {
  test_internal(name, reinterpret_cast<const char* (*)(int, void *)>(fn), p);
}

static std::array<ts::Job, MAX_SAMPLE> s_test_jobs;

uintptr_t create_mask(uint8_t v) {
  union {
    uintptr_t _v;
    uint8_t _array[sizeof _v];
  };

  for (unsigned char &i : _array) {
    i = v;
  }

  return _v;
}

template<size_t N>
void initialize(std::array<ts::Job, N>& array) {
  for (auto i = 0; i < array.size(); ++i) {
    const int c = i;
    array[i] = {
        [] (const int* p) {
          [[maybe_unused]] auto _ = create_mask(*p);
        },
        &c
    };
  }
}

#define BENCHMARK(name, type) [[clang::optnone]] void name (int i, type *p)
#define TEST(name, type) [[clang::optnone]] const char* name (int i, type *p)

#define BENCHMARK_RUN(name, p) benchmark(#name, name, &p)
#define TEST_RUN(name, p) test(#name, name, &p)
