#include <gtest/gtest.h>

#include "ts/ts.h"

using namespace ts;

constexpr size_t BASE_ITEM_COUNT = 1024 * 256;

TEST(Scheduler, Integrity) {
  scheduler sch({});
  ASSERT_TRUE(sch.start());

  std::atomic_size_t counter = 0;
  std::atomic_flag done = ATOMIC_FLAG_INIT;
  std::vector<std::vector<size_t>> buffers(sch.config().worker_count);

  sch.push(
    job::create(
      [&counter, &done, &buffers](size_t i) {
        buffers[worker::current()->id()].push_back(i);

        const auto current = ++counter;
        if (current == BASE_ITEM_COUNT) {
          done.test_and_set();
          done.notify_one();
        }
        if (current % 1024 == 0) {
          std::cout << current << '/' << BASE_ITEM_COUNT << std::endl;
        }
      }, {0, BASE_ITEM_COUNT}, nullptr
    )
  );

  do {
    done.wait(false);
  } while (!done.test());

  sch.stop(false);

  std::set<size_t> check;
  for (const auto &buffer : buffers) {
    for (auto i : buffer) {
      auto [_, b] = check.insert(i);
      EXPECT_TRUE(b) << "duplication found: " << i;
    }
  }

  for (size_t i = 0; i < BASE_ITEM_COUNT; ++i) {
    EXPECT_TRUE(check.contains(i)) << "missing found: " << i;
  }
}
