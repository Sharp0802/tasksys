# tasksys

> [!WARNING]
> `Sharp0802/tasksys` is merged into `Sharp0802/dfl`.
> Please use `dfl` instead of this project.

Welcome to `tasksys`, Coroutine-based Task System for modern C++ (C++23)

## Dependency

- [oneTBB](https://github.com/uxlfoundation/oneTBB) by UXL foundation - only for test bed
- No dependency other than STL

## Progress

- [x] Implement basic Job-system
- [x] Implement coroutine-base task class
- [ ] Create built-in IO-related awaitables

## Summary

```
#include <tasksys/Task.h>

ts::Task<> some_child_function() {
  co_return;
}

ts::Task<> some_function() {
  co_await some_child_function();
  co_return;
}

ts::WorkerGroup wg{ 16 };
some_function().schedule(wg).wait();
```

`ts::WorkerGroup` represents a group of worker.
Only workers in same group can steal each other's job.
