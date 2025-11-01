#pragma once

#include <cassert>
#include <functional>
#include <utility>

namespace ts {
  class job {
    std::function<void(size_t)> _callback;
    size_t _begin;
    size_t _end;

  public:
    job() : _begin(-1), _end(-1) {
    }

    job(std::function<void(size_t)> callback, const size_t begin, const size_t end)
      : _callback(std::move(callback)),
        _begin(begin),
        _end(end) {
    }

    job(job &&origin) noexcept {
      _callback = std::move(origin._callback);
      _begin = origin._begin;
      _end = origin._end;
      origin._begin = -1;
      origin._end = -1;
    }

    job(const job &) = delete;
    job &operator=(const job &) = delete;

    job &operator=(job &&origin) noexcept {
      if (this != &origin) {
        _callback = std::move(origin._callback);
        _begin = origin._begin;
        _end = origin._end;
        origin._begin = -1;
        origin._end = -1;
      }

      return *this;
    }

    [[nodiscard]]
    size_t size() const {
      assert(_begin != -1 && _end != -1);

      return _end - _begin;
    }

    [[nodiscard]]
    bool empty() const {
      assert(_begin != -1 && _end != -1);

      return _begin == _end;
    }

    [[nodiscard]]
    std::pair<job, job> split(const size_t at) const {
      assert(_begin != -1 && _end != -1);

      job left(_callback, _begin, _begin + at);
      job right(_callback, _begin + at, _end);
      return std::make_pair(std::move(left), std::move(right));
    }

    void call() const {
      assert(_begin != -1 && _end != -1);

      for (size_t i = _begin; i < _end; ++i) {
        _callback(i);
      }
    }
  };
}
