#pragma once

#include <functional>
#include <utility>

#include "queue.h"

namespace ts {
  struct job_config {
    size_t begin = 0;
    size_t end = 1;
    size_t batch_size = 8;
  };

  class job {
    std::function<void(size_t)> _callback;
    job_config _config;

    size_t _ref;
    job *_parent;

    friend class pool<job>;

    job(std::function<void(size_t)> callback, const job_config &config, job *parent)
      : _callback(std::move(callback)),
        _config(config),
        _ref(0),
        _parent(parent) {
    }

  public:
    [[nodiscard]]
    static job *create(
      const std::function<void(size_t)> &callback,
      const job_config &config,
      job *parent
    ) {
      if (parent) {
        __atomic_fetch_add(&parent->_ref, 1, __ATOMIC_ACQ_REL);
      }
      return mt_pool<job>::rent(std::move_if_noexcept(callback), config, parent);
    }

    job(const job &) = delete;
    job &operator=(const job &) = delete;

    void yield() {
      mt_pool<job>::yield(this);
    }

    [[nodiscard]]
    size_t size() const {
      return _config.end - _config.begin;
    }

    [[nodiscard]]
    size_t batch() const {
      return _config.batch_size;
    }

    [[nodiscard]]
    bool empty() const {
      return _config.begin == _config.end;
    }

    [[nodiscard]]
    job *split(const size_t at) {
      // left
      auto config = _config;
      _config.end = _config.begin + at;

      // right
      config.begin = _config.begin + at;
      return create(_callback, config, _parent);
    }

    [[nodiscard]]
    std::optional<job*> call() const {
      for (size_t i = _config.begin; i < _config.end; ++i) {
        _callback(i);
      }

      if (_parent && __atomic_sub_fetch(&_parent->_ref, 1, __ATOMIC_ACQ_REL) == 0) {
        return _parent;
      }

      return std::nullopt;
    }
  };
}
