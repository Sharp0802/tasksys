#pragma once

#include "LocalQueue.h"

namespace ts {
  struct HazardDtor;

  /*
   * Michael-Scott queue implementation
   */
  class GlobalQueue {
    friend struct ts::HazardDtor;

    struct Node {
      std::atomic<Node *> next;
      Job value;

      explicit Node(const Job &val) : next(nullptr), value(val) {}
    };

    alignas(64) std::atomic<Node *> head;
    alignas(64) std::atomic<Node *> tail;

  public:
    GlobalQueue();
    ~GlobalQueue();

    void push(const Job &value);
    bool pop(Job *result);
  };
}
