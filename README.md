# tasksys

Welcome to `tasksys`, Coroutine-based Task System for modern C++ (C++26)

## Dependency

- [oneTBB](https://github.com/uxlfoundation/oneTBB) by UXL foundation - only for test bed
- No dependency other than STL

## Progress

- [x] Implement basic Job-system
- [ ] Implement coroutine-base task class
- [ ] Create built-in IO-related awaitables

## Summary

- `ts::WorkerGroup`

```
#include <tasksys/Worker.h>

ts::WorkerGroup wg{ 4, 64, 64 }; // initialize worker-group
wg.push({fp, data}); // schedule job
```

`ts::WorkerGroup` represents a group of worker.
Only workers in same group can steal each other's job.

- `ts::Worker`

```
#include <tasksys/Worker.h>

ts::Worker w{ 64 }; // initialize worker
w.push({fp, data}); // schedule job
```

`ts::Worker` represents single-threaded worker.

- `ts::LocalQueue` (`<tasksys/LocalQueue.h>`) : Chase-lev work-stealing queue (fixed-size)
- `ts::GlobalQueue` (`<tasksys/GlobalQueue.h>`) : Fetch-and-Add MPMC queue (fixed-size)
