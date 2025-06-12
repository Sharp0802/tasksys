#pragma once

#include <thread>
#include <utility>

namespace ts {
  bool pin(std::jthread &thread, int cpu);
  int cpu_number() noexcept;
  int cpu_id() noexcept;

  class InsufficientThreadException : public std::exception {
    std::string _message;

  public:
    explicit InsufficientThreadException(int required) {
      _message = std::format(
          "{} thread(s) are required, but only {} thread(s) are provided",
          required,
          cpu_number());
    }

    [[nodiscard]]
    const char *what() const noexcept override {
      return _message.c_str();
    }
  };

  class InvalidStateException : public std::exception {
    std::string _message;

  public:
    explicit InvalidStateException(std::string message) : _message(std::move(message)) {
    }

    [[nodiscard]]
    const char *what() const noexcept override {
      return _message.c_str();
    }
  };
}
