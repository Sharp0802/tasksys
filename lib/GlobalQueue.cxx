#include "tasksys/GlobalQueue.h"
#include "tasksys/EBR.h"
#include <vector>

ts::GlobalQueue::GlobalQueue() {
#pragma clang diagnostic push
#pragma ide diagnostic ignored "MemoryLeak"
  Node *dummy = new Node(Job{});
#pragma clang diagnostic pop
  head.store(dummy, std::memory_order_relaxed);
  tail.store(dummy, std::memory_order_relaxed);
}

ts::GlobalQueue::~GlobalQueue() {
  Node *node = head.load(std::memory_order_relaxed);
  while (node) {
    Node *next = node->next.load(std::memory_order_relaxed);
    delete node;
    node = next;
  }
}

void ts::GlobalQueue::push(const ts::Job &value) {
#pragma clang diagnostic push
#pragma ide diagnostic ignored "MemoryLeak"
  Node *new_node = new Node(value);
#pragma clang diagnostic pop
  new_node->next.store(nullptr, std::memory_order_relaxed);

  Node *prev_tail = tail.exchange(new_node, std::memory_order_acq_rel);
  prev_tail->next.store(new_node, std::memory_order_release);
}

/*
 * Do NOT replace EBR with hazard pointer.
 * Microbenchmark says EBR is faster than hazard pointer.
 * (2025-06-12, linux 6.14.10-arch1-1, i7-12700H)
 */
bool ts::GlobalQueue::pop(ts::Job *result) {
  scoped_ebr ebr;

  Node *first;
  while (true) {
    first = head.load(std::memory_order_acquire);

    Node *next = first->next.load(std::memory_order_acquire);
    if (next == nullptr) {
      return false;
    }

    if (head.compare_exchange_weak(
        first, next,
        std::memory_order_acq_rel,
        std::memory_order_acquire)) {
      *result = next->value;
      break;
    }
  }

  ebr.retire(first, [](void *p) { delete static_cast<Node *>(p); });
  return true;
}
