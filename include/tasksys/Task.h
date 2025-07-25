#pragma once

#include <atomic>
#include <coroutine>
#include <exception>
#include <utility>
#include <variant>

#include "tasksys/WorkerGroup.h"

namespace ts {
  template<typename T>
  class Task;

  template<typename T>
  class TaskPromise {
    friend class Task<T>;

  public:
    Task<T> get_return_object();

    std::suspend_always initial_suspend() noexcept { return {}; }

    void return_value(T value) {
      _result.template emplace<T>(std::move(value));
    }

    void unhandled_exception() {
      _result.template emplace<std::exception_ptr>(std::current_exception());
    }

    auto final_suspend() noexcept {
      struct Awaiter {
        bool await_ready() noexcept { return false; }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<TaskPromise> h) noexcept {
          auto &promise = h.promise();

          promise._is_ready.store(true, std::memory_order_release);

          if (const auto continuation = promise._continuation.load(std::memory_order_acquire)) {
            return continuation;
          }

          return std::noop_coroutine();
        }

        void await_resume() noexcept {
        }
      };
      return Awaiter{};
    }

    void set_continuation(std::coroutine_handle<> continuation) {
      _continuation.store(continuation, std::memory_order_release);
    }

    T get_result() {
      if (std::holds_alternative<T>(_result)) {
        return std::get<T>(_result);
      }
      if (std::holds_alternative<std::exception_ptr>(_result)) {
        std::rethrow_exception(std::get<std::exception_ptr>(_result));
      }
      // Should not happen if awaited correctly
      throw std::runtime_error("Task result awaited before completion.");
    }

  private:
    std::variant<std::monostate, T, std::exception_ptr> _result;
    std::atomic<std::coroutine_handle<>> _continuation{nullptr};
    std::atomic<bool> _is_ready{false};
    WorkerGroup *_wg = nullptr;
  };

  template<>
  class TaskPromise<void> {
    friend class Task<void>;

  public:
    Task<void> get_return_object();
    // NOLINTNEXTLINE(*-convert-member-functions-to-static)
    std::suspend_always initial_suspend() noexcept { return {}; }
    void return_void() { _exception = nullptr; }
    void unhandled_exception() { _exception = std::current_exception(); }

    // NOLINTNEXTLINE(*-convert-member-functions-to-static)
    auto final_suspend() noexcept {
      struct Awaiter {
        [[nodiscard]]
        // NOLINTNEXTLINE(*-convert-member-functions-to-static)
        bool await_ready() const noexcept { return false; }

        // NOLINTNEXTLINE(*-convert-member-functions-to-static)
        std::coroutine_handle<> await_suspend(std::coroutine_handle<TaskPromise> h) noexcept {
          auto &promise = h.promise();

          promise._is_ready.store(true, std::memory_order_release);

          if (const auto continuation = promise._continuation.load(std::memory_order_acquire)) {
            return continuation;
          }

          return std::noop_coroutine();
        }

        void await_resume() noexcept {
        }
      };
      return Awaiter{};
    }

    void set_continuation(std::coroutine_handle<> continuation) {
      _continuation.store(continuation, std::memory_order_release);
    }

    void get_result() {
      if (_exception) {
        std::rethrow_exception(_exception);
      }
    }

  private:
    std::exception_ptr _exception;
    std::atomic<std::coroutine_handle<>> _continuation{nullptr};
    std::atomic<bool> _is_ready{false};
    WorkerGroup *_wg = nullptr;
  };


  template<typename T = void>
  class Task {
  public:
    using promise_type = TaskPromise<T>;
    using handle_type = std::coroutine_handle<promise_type>;

    explicit Task(handle_type handle) : _handle(handle) {
    }

    Task(const Task &) = delete;
    Task &operator=(const Task &) = delete;

    Task(Task &&other) noexcept : _handle(std::exchange(other._handle, nullptr)) {
    }

    ~Task() {
      if (_handle) {
        _handle.destroy();
      }
    }


    auto operator co_await() {
      struct Awaiter {
        handle_type _handle;

        bool await_ready() const noexcept {
          return !_handle || _handle.promise()._is_ready.load(std::memory_order_acquire);
        }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting_coro) noexcept {
          auto& child_promise = _handle.promise();
          child_promise.set_continuation(awaiting_coro);

          // 1. Read the worker group from the thread-local context.
          if (WorkerGroup* wg = WorkerGroup::get_current()) {
            // 2. Schedule the child task using the discovered context.
            wg->push({[](void* p) {
                // This lambda itself needs to set the context for its own execution
                auto h = handle_type::from_address(p);
                // We could store wg in the promise to make it available here,
                // or have the worker loop do it for us. The latter is cleaner.
                h.resume();
            }, _handle.address()});
          } else {
            // ERROR: A task was awaited from a context without a scheduler
            // (e.g., from the main thread without being scheduled first).
            // This is a logic error in user code. We should resume the parent
            // immediately, which will likely cause it to deadlock on `wait()`,
            // making the bug obvious.
            return awaiting_coro;
          }

          // 3. Suspend the parent.
          return std::noop_coroutine();
        }

        T await_resume() {
          return _handle.promise().get_result();
        }
      };
      return Awaiter{_handle};
    }

    Task &schedule(WorkerGroup &wg) & {
      if (_handle) {
        wg.push(
          {
            [](void *p) {
              handle_type::from_address(p)->resume();
            },
            _handle.address()
          });
      }

      return *this;
    }

    Task &&schedule(WorkerGroup &wg) && {
      if (_handle) {
        wg.push(
          {
            [](void *p) {
              handle_type::from_address(p).resume();
            },
            _handle.address()
          });
      }

      return std::move(*this);
    }

    auto wait() & -> decltype(auto) {
      if (!_handle) {
        throw std::runtime_error("wait() called on a moved-from Task");
      }

      while (!_handle.promise()._is_ready.load(std::memory_order_acquire)) {
        _mm_pause();
      }

      if constexpr (std::is_void_v<T>) {
        _handle.promise().get_result();
      } else {
        return _handle.promise().get_result();
      }
    }

    auto wait() && -> decltype(auto) {
      if (!_handle) {
        throw std::runtime_error("wait() called on a moved-from Task");
      }

      while (!_handle.promise()._is_ready.load(std::memory_order_acquire)) {
        _mm_pause();
      }

      if constexpr (std::is_void_v<T>) {
        _handle.promise().get_result();
      } else {
        return _handle.promise().get_result();
      }
    }

  private:
    handle_type _handle;
  };

  template<typename T>
  Task<T> TaskPromise<T>::get_return_object() {
    return Task<T>{std::coroutine_handle<TaskPromise>::from_promise(*this)};
  }

  inline Task<> TaskPromise<void>::get_return_object() {
    return Task{std::coroutine_handle<TaskPromise>::from_promise(*this)};
  }
}
