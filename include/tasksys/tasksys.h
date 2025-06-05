#pragma once

#include <cassert>
#include <coroutine>
#include <cstddef>
#include <atomic>
#include <optional>
#include <variant>
#include <queue>
#include <thread>
#include "ringbuffer.h"
#include "workqueue.h"
#include "threadpool.h"

namespace ts {

  template<typename T>
  struct Task {
    struct promise_type;

    using Handle = std::coroutine_handle<promise_type>;

    Handle _coroutine;


    Task(Handle h) : _coroutine(h) {}
    ~Task() {
      if (_coroutine) _coroutine.destroy();
    }

    [[nodiscard]] T await() {
      while (!_coroutine.done()) {
        _coroutine.resume();
      }

      auto &promise = _coroutine.promise();
      if (auto *e = std::get_if<std::exception_ptr>(&promise._value)) {
        std::rethrow_exception(*e);
      }

      return std::get<T>(promise._value);
    }

    struct promise_type {
      std::variant<T, std::exception_ptr> _value;

      auto get_return_object() {
        return Task{
            Handle::from_promise(*this)
        };
      }

      std::suspend_always initial_suspend() {
        return {};
      }

      std::suspend_always final_suspend() noexcept {
        return {};
      }

      void return_value(T v) {
        _value = v;
      }

      void unhandled_exception() {
        _value = std::current_exception();
      }
    };
  };
}
