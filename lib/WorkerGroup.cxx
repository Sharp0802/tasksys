#include "tasksys/WorkerGroup.h"

#include "tasksys/Memory.h"

namespace {
  thread_local ts::WorkerGroup *_wg = nullptr;
}

namespace ts {
  void WorkerGroup::set_current(WorkerGroup &wg) {
    _wg = &wg;
  }

  WorkerGroup * WorkerGroup::get_current() {
    return _wg;
  }

  WorkerGroup::WorkerGroup(const size_t size): _worker_count(size), _queues(size) {
    _workers = alloc<Worker>(size);
    for (size_t i = 0; i < size; ++i) {
      new (&_workers[i]) Worker(i, *this, _queues, _global);
    }

    for (size_t i = 0; i < size; ++i) {
      _worker_map.emplace(_workers[i].thread_id(), &_workers[i]);
    }
  }

  WorkerGroup::~WorkerGroup() {
    stop();

    for (auto i = 0; i < _worker_count; ++i) {
      _workers[i].~Worker();
    }

    free(_workers);
  }

  void WorkerGroup::stop() {
    for (auto i = 0; i < _worker_count; ++i) {
      _workers[i].stop();
    }
  }

  void WorkerGroup::push(const Job &job) {
    if (const auto iter = _worker_map.find(std::this_thread::get_id()); iter != _worker_map.end()) {
      iter->second->push(job);
      return;
    }

    while (!_global.push(job)) {
      std::this_thread::yield();
    }
  }
}
