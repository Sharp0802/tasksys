#include <mutex>
#include <chrono>
#include <execution>
#include "tasksys/Worker.h"
#include "tasksys/memory.h"
#include "tasksys/cpu.h"

using namespace std::chrono_literals;

static std::atomic_int g_cpu_offset = 1; // 0 is for main thread

thread_local ts::Worker *ts::Worker::tl_this_worker = nullptr;

void ts::Worker::run() {
  tl_this_worker = this;

  {
    std::unique_lock lock(_mutex);
    _cv.wait(lock);
  }

  while (!_token.stop_requested()) {
    Job job{};

    for (auto i = 0; i < 1000; ++i) {
      if (_queue.pop(&job)) {
        break;
      }
      if (_group._queue.pop(&job)) {
        break;
      }
      if ((i & 0xF) == 0 && steal(&job)) {
        break;
      }

      _mm_pause();
    }
    if (job) {
      job();
      continue;
    }

    std::this_thread::yield();
  }

  _group.notify_closed();
}

bool ts::Worker::steal(ts::Job *job) noexcept {
  for (size_t i = 1; i < _group._size; ++i) {
    auto &worker = _group._workers[(_index + i) % _group._size];
    if (worker._queue.steal(job)) {
      return true;
    }
  }

  return false;
}

ts::Worker::Worker(
    WorkerGroup &group,
    size_t index,
    std::stop_source &sts,
    size_t size)
    : _group(group),
      _index(index),
      _queue(size),
      _token(sts.get_token()),
      _thread(&Worker::run, this) {
#ifndef PIN_WORKER
  group._worker_map[_thread.get_id()] = this;
#endif
}

bool ts::Worker::push(const ts::Job &job) noexcept {
  return _queue.push(job);
}

#ifdef PIN_WORKER
bool ts::Worker::pin(int cpu) noexcept {
  return ts::pin(_thread, cpu);
}
#endif

void ts::Worker::join() noexcept {
  if (_thread.joinable()) {
    _thread.join();
  }
}

void ts::Worker::start() noexcept {
  _cv.notify_all();
}

void ts::WorkerGroup::notify_closed() noexcept {
  if (--_available == 0) {
    std::scoped_lock lock(_mutex);
    _cv.notify_all();
  }
}

#ifdef PIN_WORKER
bool ts::WorkerGroup::is_bound(int cpu) const noexcept {
  // todo : optimize for big-little
  cpu -= _cpu_offset;
  return 0 <= cpu && cpu < _size;
}

int ts::WorkerGroup::map_index(int cpu) const noexcept {
  // todo : optimize for big-little
  return cpu - _cpu_offset;
}
#endif

void ts::WorkerGroup::dispose() noexcept {
  _cv.notify_all();

  stop();
  join(1000ms);

  for (size_t i = 0; i < _size; ++i) {
    //_workers[i].join(); <- jthread automatically joins at destruction
    _workers[i].~Worker();
  }

  ts::free(_workers);
}

ts::WorkerGroup::WorkerGroup(int size, size_t capacity, size_t local_capacity)
    : _size(size),
      _available(size),
      _queue(capacity) {

#ifdef PIN_WORKER
  _cpu_offset = g_cpu_offset.fetch_add(size);
  if (_cpu_offset + size > ts::cpu_number()) {
    throw ts::InsufficientThreadException(_cpu_offset + size);
  }

  bool pinned = true;
#endif

  _workers = ts::alloc<Worker>(size);
  for (auto i = 0; i < size; ++i) {
#ifdef PIN_WORKER
    auto *p =
#endif

    new(&_workers[i]) Worker(*this, i, _sts, local_capacity);

#ifdef PIN_WORKER
    pinned &= p->pin(_cpu_offset + i);
#endif
  }

#ifdef PIN_WORKER
  if (!pinned) {
    dispose();
    throw ts::InvalidStateException("failed to pin logical thread to physical thread");
  }
#endif
}

ts::WorkerGroup::~WorkerGroup() {
  dispose();
}

void ts::WorkerGroup::start() noexcept {
  for (auto i = 0; i < _size; ++i) {
    _workers[i].start();
  }
}

void ts::WorkerGroup::push(const ts::Job &job) noexcept {
  if (Worker::tl_this_worker) {
    Worker::tl_this_worker->push(job);
    return;
  }

#ifdef PIN_WORKER
  auto id = ts::cpu_id();
  if (is_bound(id)) {
    _workers[map_index(id)].push(job);
    return;
  }
#else
  auto it = _worker_map.find(std::this_thread::get_id());
  if (it != _worker_map.end()) {
    it->second->push(job);
  }
#endif

  while (!_queue.push(job)) {
    std::this_thread::yield();
  }
}

bool ts::WorkerGroup::join(std::chrono::milliseconds timeout) noexcept {
  std::unique_lock lock(_mutex);
  if (_cv.wait_for(lock, timeout, [this]() { return _available == 0; })) {
    return true;
  }

  return false;
}

void ts::WorkerGroup::stop() noexcept {
  _sts.request_stop();
}
