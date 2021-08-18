#pragma once

// Workaround for libubox/utils.h
#undef fallthrough

#include <nlohmann/json.hpp>

// Workaround for libubox/utils.h
#ifndef fallthrough
# if __has_attribute(__fallthrough__)
#  define fallthrough __attribute__((__fallthrough__))
# else
#  define fallthrough do {} while (0)  /* fallthrough */
# endif
#endif

using json = nlohmann::json;
