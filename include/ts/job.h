#pragma once

#include <functional>
#include <utility>

#include "queue.h"

namespace ts {
  class job {
    std::function<void(size_t)> _callback;
    size_t _begin;
    size_t _end;

    size_t _ref;
    job *_parent;

    friend class pool<job>;

    job(std::function<void(size_t)> callback, const size_t begin, const size_t end, job *parent)
      : _callback(std::move(callback)),
        _begin(begin),
        _end(end),
        _ref(0),
        _parent(parent) {
    }

  public:
    [[nodiscard]]
    static job *create(
      const std::function<void(size_t)> &callback,
      const size_t begin,
      const size_t end,
      job *parent
    ) {
      if (parent) {
        __atomic_fetch_add(&parent->_ref, 1, __ATOMIC_ACQ_REL);
      }
      return mt_pool<job>::rent(std::move_if_noexcept(callback), begin, end, parent);
    }

    job(const job &) = delete;
    job &operator=(const job &) = delete;

    void yield() {
      mt_pool<job>::yield(this);
    }

    [[nodiscard]]
    size_t size() const {
      return _end - _begin;
    }

    [[nodiscard]]
    bool empty() const {
      return _begin == _end;
    }

    [[nodiscard]]
    job *split(const size_t at) {
      // left
      _end = _begin + at;

      // right
      return create(_callback, _begin + at, _end, _parent);
    }

    [[nodiscard]]
    std::optional<job*> call() const {
      for (size_t i = _begin; i < _end; ++i) {
        _callback(i);
      }

      if (_parent && __atomic_sub_fetch(&_parent->_ref, 1, __ATOMIC_ACQ_REL) == 0) {
        return _parent;
      }

      return std::nullopt;
    }
  };
}
