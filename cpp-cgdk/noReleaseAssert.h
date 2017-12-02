#pragma once

#ifndef _DEBUG
#  ifndef NDEBUG
#    define NDEBUG    // server-side runner doesn't define this
#  endif
#endif

#include <cassert>