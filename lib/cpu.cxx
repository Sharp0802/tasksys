#include "tasksys/cpu.h"

#ifdef _WIN32
#include <windows.h>
#elif __linux__
#include <sched.h>
#include <pthread.h>
#else
#error Unrecognized target platform
#endif

#include <thread>

namespace {
  bool pin_thread(std::jthread &thr, uint32_t cpu_id) {
#ifdef _WIN32
    HANDLE handle = static_cast<HANDLE>(thr.native_handle());
    DWORD_PTR mask = static_cast<DWORD_PTR>(1) << cpu_id;
    DWORD_PTR result = SetThreadAffinityMask(handle, mask);
    return (result != 0);
#else
    auto handle = thr.native_handle();

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    int err = pthread_setaffinity_np(handle, sizeof(cpu_set_t), &cpuset);
    return (err == 0);
#endif
  }

  int __cpu_id() {
#ifdef _WIN32
    return static_cast<int>(GetCurrentProcessorNumber());
#elif defined(__linux__)
    int cpu = sched_getcpu();
    return (cpu >= 0 ? cpu : -1);
#else
    return -1;
#endif
  }

  int __cpu_number() {
#ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return static_cast<unsigned int>(sysinfo.dwNumberOfProcessors);
#elif defined(__linux__)
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return (n > 0 ? static_cast<int>(n) : 1);
#else
    unsigned int hc = std::thread::hardware_concurrency();
    return (hc > 0 ? hc : 1);
#endif
  }
}

static int s_n = -1;
thread_local static int tl_id = -1;

bool ts::pin(std::jthread &thread, int cpu) {
  return pin_thread(thread, cpu);
}

int ts::cpu_number() noexcept {
  if (s_n != -1) {
    return s_n;
  }

  return s_n = __cpu_number();
}

int ts::cpu_id() noexcept {
  if (tl_id != -1) {
    return tl_id;
  }

  return tl_id = __cpu_id();
}
