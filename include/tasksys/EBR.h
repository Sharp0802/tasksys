#pragma once

namespace ts {

  class scoped_ebr {
    /*
     * do NOT change naming convention:
     * snake-case is intended to blend it with STL-style fences (such as std::scoped_lock)
     */

    bool _init;

  public:
    scoped_ebr();
    ~scoped_ebr();

    void retire(void *p, void (*del)(void *)) noexcept;
  };

}
