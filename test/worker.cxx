#include <atomic>
#include <chrono>
#include <numeric>
#include <thread>
#include <vector>
#include <gtest/gtest.h>

#include "ts/ts.h"

using namespace ts;

constexpr size_t THREAD_COUNT = 8;
constexpr size_t BASE_ITEM_COUNT = 1024 * 16;

// --- Test ts::worker ---

TEST(WorkerTest, BasicDispatch) {
  std::vector<std::unique_ptr<worker>> workers{};
  vyukov<job*> global(4096);

  workers.reserve(1);
  workers.emplace_back(std::make_unique<worker>(workers, global, 4096));

  workers[0]->start();

  std::atomic_flag done = ATOMIC_FLAG_INIT;

  ASSERT_TRUE(
    global.push(job::create([&done] (size_t) { done.test_and_set(); }, {}, nullptr))
  );

  while (!done.test()) {
    _mm_pause();
  }

  global.kill();
  workers[0]->stop();
}

TEST(WorkerTest, GrainedIntegrity) {
  std::vector<std::unique_ptr<worker>> workers{};
  std::array<std::vector<size_t>, THREAD_COUNT> buffer{};

  std::atomic_size_t counter = 0;
  std::atomic_flag alarm = ATOMIC_FLAG_INIT;

  vyukov<job*> global(4096);

  // prepare workers
  workers.reserve(THREAD_COUNT);
  for (size_t i = 0; i < THREAD_COUNT; ++i) {
    workers.emplace_back(std::make_unique<worker>(workers, global, 4096));
  }
  for (size_t i = 0; i < THREAD_COUNT; ++i) {
    workers[i]->start();
  }

  // push works
  for (size_t i = 0; i < BASE_ITEM_COUNT; ++i) {
    const auto ci = i;
    global.blocking_push(
      job::create(
        [&buffer, &counter, &alarm, ci](size_t) {
          buffer[worker::current()->id()].push_back(ci);

          const auto current = ++counter;
          if (current == BASE_ITEM_COUNT) {
            alarm.test_and_set(release);
            alarm.notify_all();
          }
          if (current % 1024 == 0) {
            std::cout << current << '/' << BASE_ITEM_COUNT << std::endl;
          }
        }, {}, nullptr));
  }

  // waiting for completion
  do {
    alarm.wait(false);
  } while (!alarm.test_and_set());

  // finalization
  global.kill();
  for (size_t i = 0; i < THREAD_COUNT; ++i) {
    workers[i]->stop();
  }

  // verify
  std::set<size_t> check;
  for (size_t i = 0; i < THREAD_COUNT; ++i) {
    for (const auto v : buffer[i]) {
      const auto [_, b] = check.insert(v);
      EXPECT_TRUE(b) << "duplication found: " << v;
    }
  }
  for (size_t i = 0; i < BASE_ITEM_COUNT; ++i) {
    EXPECT_TRUE(check.contains(i)) << "missing found: " << i;
  }

  // summarize
  for (size_t i = 0; i < THREAD_COUNT; ++i) {
    std::cout << "thread[" << i << "] = " << buffer[i].size() << std::endl;
  }
}

TEST(WorkerTest, CoarsedIntegrity) {
  std::vector<std::unique_ptr<worker>> workers{};
  std::array<std::vector<size_t>, THREAD_COUNT> buffer{};

  std::atomic_size_t counter = 0;
  std::atomic_flag alarm = ATOMIC_FLAG_INIT;

  vyukov<job*> global(4096);

  // prepare workers
  workers.reserve(THREAD_COUNT);
  for (size_t i = 0; i < THREAD_COUNT; ++i) {
    workers.emplace_back(std::make_unique<worker>(workers, global, 4096));
  }
  for (size_t i = 0; i < THREAD_COUNT; ++i) {
    workers[i]->start();
  }

  // push works
  global.blocking_push(
    job::create(
      [&buffer, &counter, &alarm](const size_t i) {
        buffer[worker::current()->id()].push_back(i);

        const auto current = ++counter;
        if (current == BASE_ITEM_COUNT) {
          alarm.test_and_set(release);
          alarm.notify_all();
        }
        if (current % 1024 == 0) {
          std::cout << current << '/' << BASE_ITEM_COUNT << std::endl;
        }
      }, { 0, BASE_ITEM_COUNT }, nullptr)
  );

  // waiting for completion
  do {
    alarm.wait(false);
  } while (!alarm.test_and_set());

  // finalization
  global.kill();
  for (size_t i = 0; i < THREAD_COUNT; ++i) {
    workers[i]->stop();
  }

  // verify
  std::set<size_t> check;
  for (size_t i = 0; i < THREAD_COUNT; ++i) {
    for (const auto v : buffer[i]) {
      const auto [_, b] = check.insert(v);
      EXPECT_TRUE(b) << "duplication found: " << v;
    }
  }
  for (size_t i = 0; i < BASE_ITEM_COUNT; ++i) {
    EXPECT_TRUE(check.contains(i)) << "missing found: " << i;
  }

  // summarize
  for (size_t i = 0; i < THREAD_COUNT; ++i) {
    std::cout << "thread[" << i << "] = " << buffer[i].size() << std::endl;
  }
}
