#include "tasksys/EBR.h"
#include <cstddef>
#include <atomic>
#include <vector>
#include <array>
#include <cassert>

constexpr size_t MAX_THREAD = 256;
constexpr size_t RETIRE_THRESHOLD = 96;

static std::atomic_uint64_t s_global_epoch{0};

static std::array<std::atomic<std::atomic_uint64_t *>, MAX_THREAD> s_local_epochs;

static std::atomic_uint32_t s_thread_id;
thread_local uint32_t tl_thread_id;

thread_local std::atomic_uint64_t tl_local_epoch{0};
thread_local bool tl_in_critical = false;
thread_local std::vector<std::tuple<uint64_t, void *, void (*)(void *)>> tl_retires;

[[maybe_unused]]
thread_local struct ThreadLocalHooker {
  ThreadLocalHooker() {
    tl_thread_id = s_thread_id.fetch_add(1);
    assert(tl_thread_id < MAX_THREAD);

    s_local_epochs[tl_thread_id] = &tl_local_epoch;
  }

  ~ThreadLocalHooker() {
    s_local_epochs[tl_thread_id] = nullptr;

    for (auto &[_, p, deleter]: tl_retires) {
      deleter(p);
    }

    tl_retires.clear();
  }
} tl_hooker;

uint64_t min_epoch() {
  uint64_t min = UINT64_MAX;
  for (auto &epoch : s_local_epochs) {
    if (epoch == nullptr) {
      continue;
    }

    auto copy = epoch.load()->load();
    if (copy < min) {
      min = copy;
    }
  }

  return min;
}

void try_advance_epoch() {
  auto current = s_global_epoch.load();
  if (min_epoch() >= current) {
    s_global_epoch.compare_exchange_strong(current, current + 1, std::memory_order_acq_rel);
  }
}

void try_reclaim() {
  auto min = min_epoch();

  for (auto it = tl_retires.begin(); it != tl_retires.end(); ) {
    auto &[epoch, p, deleter] = *it;
    if (epoch + 2 <= min) {
      deleter(p);
      it = tl_retires.erase(it);
    } else {
      ++it;
    }
  }
}

ts::scoped_ebr::scoped_ebr() {
  if (tl_in_critical) {
    _init = false;
    return;
  }

  _init = true;

  tl_local_epoch.store(s_global_epoch.load(std::memory_order_acquire), std::memory_order_release);
  tl_in_critical = true;
}

ts::scoped_ebr::~scoped_ebr() {
  if (!_init) {
    return;
  }

  tl_in_critical = false;
}

void ts::scoped_ebr::retire(void *p, void (*del)(void *)) noexcept {
  tl_retires.emplace_back(tl_local_epoch.load(), p, del);
  if (tl_retires.size() >= RETIRE_THRESHOLD) {
    try_advance_epoch();
    try_reclaim();
  }
}
