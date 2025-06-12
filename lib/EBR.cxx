#include "tasksys/EBR.h"
#include <atomic>
#include <cstddef>
#include <vector>
#include <cassert>
#include <thread>
#include <mutex>
#include <algorithm>

constexpr size_t MAX_THREADS = 128;
constexpr size_t RETIRE_SHARDS = 16;
constexpr size_t GLOBAL_RETIRE_THRESHOLD = 64;

struct alignas(64) ThreadEpoch {
  std::atomic<uint64_t> epoch{UINT64_MAX};
};

struct RetireNode {
  uint64_t epoch;
  void *ptr;
  void (*del)(void *);
  uint32_t shard;
};

static std::atomic<uint64_t> s_global_epoch{1};
static ThreadEpoch s_local_epochs[MAX_THREADS];
static std::atomic<uint32_t> s_next_id{0};
static std::mutex s_reg_mutex;
static std::vector<uint32_t> s_active_ids;

static std::array<std::vector<RetireNode>, RETIRE_SHARDS> s_retire_shards;
static std::array<std::mutex, RETIRE_SHARDS> s_shard_mutexes;
static std::atomic<size_t> s_retire_count{0};

thread_local uint32_t tl_id;
thread_local bool tl_in_crit = false;

[[maybe_unused]]
thread_local struct ThreadRegister {
  ThreadRegister() {
    tl_id = s_next_id.fetch_add(1, std::memory_order_relaxed);
    assert(tl_id < MAX_THREADS);
    std::lock_guard lk(s_reg_mutex);
    s_active_ids.push_back(tl_id);
    s_local_epochs[tl_id].epoch.store(UINT64_MAX, std::memory_order_relaxed);
  }

  ~ThreadRegister() {
    {
      std::lock_guard lk(s_reg_mutex);
      auto it = std::find(s_active_ids.begin(), s_active_ids.end(), tl_id);
      if (it != s_active_ids.end()) {
        s_active_ids.erase(it);
      }
    }
    // Clear epoch so dead thread does not block reclamation
    s_local_epochs[tl_id].epoch.store(UINT64_MAX, std::memory_order_relaxed);
  }
} tl_thread_register;

inline uint64_t scan_min_epoch() {
  uint64_t m = UINT64_MAX;
  std::lock_guard lk(s_reg_mutex);
  for (auto id: s_active_ids) {
    uint64_t e = s_local_epochs[id].epoch.load(std::memory_order_relaxed);
    if (e < m) {
      m = e;
    }
  }
  return m;
}

void try_advance_and_reclaim() {
  size_t cnt = s_retire_count.load(std::memory_order_relaxed);
  if (cnt < GLOBAL_RETIRE_THRESHOLD) {
    return;
  }

  // Advance global epoch if possible
  uint64_t cur = s_global_epoch.load(std::memory_order_relaxed);
  uint64_t min = scan_min_epoch();
  if (min >= cur) {
    s_global_epoch.compare_exchange_strong(cur, cur + 1, std::memory_order_acq_rel);
  }

  std::vector<RetireNode> pending;
  pending.reserve(cnt);
  for (uint32_t s = 0; s < RETIRE_SHARDS; ++s) {
    std::lock_guard lk(s_shard_mutexes[s]);
    auto &vec = s_retire_shards[s];
    pending.insert(pending.end(), vec.begin(), vec.end());
    vec.clear();
  }
  s_retire_count.store(0, std::memory_order_relaxed);

  uint64_t new_min = scan_min_epoch();
  for (auto &node: pending) {
    if (node.epoch + 2 <= new_min) {
      node.del(node.ptr);
    } else {
      // put back into its original shard
      auto &sh = s_retire_shards[node.shard];
      std::lock_guard lk(s_shard_mutexes[node.shard]);
      sh.push_back(node);
      s_retire_count.fetch_add(1, std::memory_order_relaxed);
    }
  }
}

ts::scoped_ebr::scoped_ebr() {
  if (tl_in_crit) {
    _init = false;
    return;
  }

  _init = true;
  uint64_t e = s_global_epoch.load(std::memory_order_relaxed);
  s_local_epochs[tl_id].epoch.store(e, std::memory_order_relaxed);
  tl_in_crit = true;
}

ts::scoped_ebr::~scoped_ebr() {
  if (!_init) {
    return;
  }

  tl_in_crit = false;
  s_local_epochs[tl_id].epoch.store(UINT64_MAX, std::memory_order_relaxed);
}

void ts::scoped_ebr::retire(void *p, void(*del)(void *)) noexcept {
  uint64_t e = s_local_epochs[tl_id].epoch.load(std::memory_order_relaxed);
  uint32_t shard = tl_id % RETIRE_SHARDS;
  {
    auto &vec = s_retire_shards[shard];
    std::lock_guard lk(s_shard_mutexes[shard]);
    vec.push_back({e, p, del, shard});
  }
  if (s_retire_count.fetch_add(1, std::memory_order_relaxed) + 1 >= GLOBAL_RETIRE_THRESHOLD) {
    try_advance_and_reclaim();
  }
}
