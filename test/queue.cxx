#include <atomic>
#include <chrono>
#include <numeric>
#include <thread>
#include <vector>
#include <gtest/gtest.h>

#include "ts/queue.h"

using namespace ts;

constexpr size_t THREAD_COUNT = 8;
constexpr size_t BASE_ITEM_COUNT = 1024 * 1024 * 8;

// --- Test ts::queue ---
// (Single-threaded manipulation)

TEST(QueueTest, BasicPushPop) {
  queue<int> q;
  EXPECT_TRUE(q.empty());
  EXPECT_EQ(q.size(), 0);

  q.push(10);
  EXPECT_FALSE(q.empty());
  EXPECT_EQ(q.size(), 1);

  q.push(20);
  EXPECT_EQ(q.size(), 2);

  const auto val1 = q.pop();
  EXPECT_TRUE(val1.has_value());
  EXPECT_EQ(val1.value(), 10);
  EXPECT_EQ(q.size(), 1);

  const auto val2 = q.pop();
  EXPECT_TRUE(val2.has_value());
  EXPECT_EQ(val2.value(), 20);
  EXPECT_EQ(q.size(), 0);
  EXPECT_TRUE(q.empty());
}


// --- Test ts::faa ---
// (Multi-Producer, Multi-Consumer)

TEST(FAATest, BasicPushPop) {
  vyukov<int> q(8);
  EXPECT_TRUE(q.push(10));
  EXPECT_TRUE(q.push(20));

  const auto val1 = q.pop();
  EXPECT_TRUE(val1.has_value());
  EXPECT_EQ(val1, 10);

  const auto val2 = q.pop();
  EXPECT_TRUE(val2.has_value());
  EXPECT_EQ(val2, 20);

  EXPECT_FALSE(q.pop().has_value());
}

TEST(FAATest, FullAndEmpty) {
  vyukov<int> q(2);
  EXPECT_TRUE(q.push(1));
  EXPECT_TRUE(q.push(2));

  // Queue is full. Note: MPMC fullness is racy, so we check
  // a few times. This test is single-threaded, so it *should*
  // fail, but the check in push is `head - tail`, so it's fine.
  EXPECT_FALSE(q.push(3));

  EXPECT_EQ(q.pop(), 1);
  EXPECT_EQ(q.pop(), 2);

  // Queue is empty
  EXPECT_FALSE(q.pop().has_value());
}

TEST(FAATest, Integrity) {
  static constexpr size_t QUEUE_SIZE = 128;
  static constexpr size_t THREAD_SIZE = BASE_ITEM_COUNT / THREAD_COUNT;

  static_assert(BASE_ITEM_COUNT % THREAD_COUNT == 0);

  vyukov<size_t> q(QUEUE_SIZE);

  std::vector<std::jthread> writers;
  writers.reserve(THREAD_COUNT);
  for (size_t i = 0; i < THREAD_COUNT; ++i) {
    size_t ci = i;
    writers.emplace_back(
      [&q, ci] {
        for (size_t item = 0; item < THREAD_SIZE;) {
          if (q.push(item + ci * THREAD_SIZE)) {
            item++;
          }
        }
      }
    );
  }

  std::array<std::vector<size_t>, THREAD_COUNT> buffer;

  std::vector<std::jthread> readers;
  readers.reserve(THREAD_COUNT);
  for (size_t i = 0; i < THREAD_COUNT; ++i) {
    size_t ci = i;
    readers.emplace_back(
      [&q, &buffer, ci] {
        for (size_t item = 0; item < THREAD_SIZE;) {
          if (auto v = q.pop()) {
            buffer[ci].push_back(v.value());
            item++;
          }
        }
      }
    );
  }

  for (auto &reader : readers) {
    reader.join();
  }
  for (auto &writer : writers) {
    writer.join();
  }

  std::set<size_t> check;
  for (size_t i = 0; i < THREAD_COUNT; ++i) {
    for (auto v : buffer[i]) {
      const auto [_, b] = check.insert(v);
      EXPECT_TRUE(b) << "duplication found: " << v;
    }
  }

  for (size_t i = 0; i < THREAD_COUNT * THREAD_SIZE; ++i) {
    EXPECT_TRUE(check.contains(i)) << "missing found: " << i;
  }
}


// --- Test ts::chaselev ---
// (Single Owner (push/take), Multi-Stealer (steal))

TEST(ChaseLevTest, BasicPushTake) {
  chaselev<int> d(8);
  EXPECT_TRUE(d.push(10));
  EXPECT_TRUE(d.push(20)); // Pushes [10, 20]

  const auto val1 = d.take(); // LIFO
  EXPECT_TRUE(val1.has_value());
  EXPECT_EQ(val1.value(), 20);

  const auto val2 = d.take();
  EXPECT_TRUE(val2.has_value());
  EXPECT_EQ(val2.value(), 10);

  EXPECT_FALSE(d.take().has_value());
}

TEST(ChaseLevTest, BasicPushSteal) {
  chaselev<int> d(8);
  EXPECT_TRUE(d.push(10));
  EXPECT_TRUE(d.push(20)); // Pushes [10, 20]

  // Steal is FIFO
  const auto val1 = d.steal();
  EXPECT_TRUE(val1.has_value());
  EXPECT_EQ(val1.value(), 10);

  const auto val2 = d.steal();
  EXPECT_TRUE(val2.has_value());
  EXPECT_EQ(val2.value(), 20);

  EXPECT_FALSE(d.steal().has_value());
}

TEST(ChaseLevTest, FullAndEmpty) {
  chaselev<int> d(2);
  EXPECT_TRUE(d.push(1));
  EXPECT_TRUE(d.push(2));
  EXPECT_FALSE(d.push(3)); // Full

  EXPECT_EQ(d.take().value(), 2);
  EXPECT_EQ(d.take().value(), 1);
  EXPECT_FALSE(d.take().has_value()); // Empty
}

TEST(ChaseLevTest, RaceOnLastItem) {
  chaselev<int> d(8);
  d.push(100);

  // This is a single-threaded test of the race path.
  // Owner takes, but no stealers interfere.
  const auto val = d.take();
  EXPECT_TRUE(val.has_value());
  EXPECT_EQ(val.value(), 100);
  EXPECT_FALSE(d.take().has_value());
}

TEST(ChaseLevTest, OneOwnerManyStealers) {
  static constexpr size_t QUEUE_SIZE = 128;
  static constexpr size_t ITEM_COUNT = BASE_ITEM_COUNT;

  chaselev<size_t> d(QUEUE_SIZE);

  std::atomic_flag start = false;
  std::atomic_flag stop = false;

  std::jthread owner(
    [&d, &start] {
      for (auto i = 0; i < QUEUE_SIZE; ++i) {
        EXPECT_TRUE(d.push(i));
      }

      start.test_and_set(release);
      start.notify_all();

      for (auto i = QUEUE_SIZE; i < ITEM_COUNT;) {
        // push and
        if (d.push(i))
          i++;

        // take and
        if (i & 1) {
          if (auto v = d.take()) {
            // push
            while (!d.push(v.value())) {
              _mm_pause();
            }
          }
        }
      }
    }
  );

  std::array<std::vector<size_t>, THREAD_COUNT> buffer;

  std::atomic_size_t stolen = 0;
  std::vector<std::jthread> readers;
  readers.reserve(THREAD_COUNT);
  for (size_t i = 0; i < THREAD_COUNT; ++i) {
    const auto ci = i;
    readers.emplace_back(
      [&d, &start, &stop, &buffer, &stolen, ci] {
        start.wait(false, acquire);

        while (!stop.test()) {
          if (const auto v = d.steal()) {
            buffer[ci].push_back(v.value());

            const auto s = stolen.fetch_add(1, std::memory_order_acq_rel) + 1;
            if (s >= ITEM_COUNT) {
              stop.test_and_set(release);
            }
          }

          _mm_pause();
        }
      }
    );
  }

  owner.join();
  for (auto &reader : readers) {
    reader.join();
  }

  std::set<size_t> check;
  for (size_t i = 0; i < THREAD_COUNT; ++i) {
    for (const auto v : buffer[i]) {
      const auto [_, b] = check.insert(v);
      EXPECT_TRUE(b) << "duplication found: " << v;
    }
  }

  for (size_t i = 0; i < ITEM_COUNT; ++i) {
    EXPECT_TRUE(check.contains(i)) << "missing found: " << i;
  }
}
