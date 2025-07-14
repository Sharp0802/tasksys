#pragma once

#ifdef _DEBUG
#include <cassert>
#define Assert(x) assert(x)
#else
#define Assert(x)
#endif
