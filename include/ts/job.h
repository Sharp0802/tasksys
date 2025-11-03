#pragma once

#include <cassert>
#include <functional>
#include <utility>

namespace ts {
  class job {
    std::function<void(size_t)> _callback;

    bool _valid;
    size_t _begin;
    size_t _end;

    size_t _ref;
    job *_parent;

    void from(job &&origin) noexcept {
      _callback = std::move(origin._callback);
      _valid = origin._valid;
      _begin = origin._begin;
      _end = origin._end;
      _ref = origin._ref;
      _parent = origin._parent;

      origin._valid = false;
#if !NDEBUG
      origin._begin = 0;
      origin._end = 0;
      origin._ref = 0;
      origin._parent = nullptr;
#endif
    }

  public:
    job()
      : _valid(false),
        _begin(0),
        _end(0),
        _ref(0),
        _parent(nullptr) {
    }

    job(std::function<void(size_t)> callback, const size_t begin, const size_t end, job *parent)
      : _callback(std::move(callback)),
        _valid(true),
        _begin(begin),
        _end(end),
        _ref(0),
        _parent(parent) {
    }

    job(job &&origin) noexcept {
      from(std::forward<job>(origin));
    }

    job(const job &) = delete;
    job &operator=(const job &) = delete;

    job &operator=(job &&origin) noexcept {
      if (this != &origin) {
        from(std::forward<job>(origin));
      }

      return *this;
    }

    [[nodiscard]]
    size_t size() const {
      assert(_valid);
      return _end - _begin;
    }

    [[nodiscard]]
    bool empty() const {
      assert(_valid);
      return _begin == _end;
    }

    [[nodiscard]]
    std::pair<job, job> split(const size_t at) const {
      assert(_valid);

      if (_parent) {
        __atomic_fetch_add(&_parent->_ref, 1, __ATOMIC_RELEASE);
      }

      job left(_callback, _begin, _begin + at, _parent);
      job right(_callback, _begin + at, _end, _parent);
      return std::make_pair(std::move(left), std::move(right));
    }

    [[nodiscard]]
    std::optional<job*> call() const {
      assert(_valid);

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
