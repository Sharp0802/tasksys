#include "tasksys/Job.h"

namespace ts {
  void Job::operator()() const {
    Assert(_fn && "_fn cannot be null");
    _fn(_data);
  }
}
