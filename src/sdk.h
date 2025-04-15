#pragma once

#ifdef WIN32
#define _WIN32_WINNT 0x601
#include <sdkddkver.h>
#endif

#define BOOST_BEAST_USE_STD_STRING_VIEW