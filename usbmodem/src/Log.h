#pragma once

#include <cstdio>

#define LOGD(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) fprintf(stderr, "error: " fmt, ##__VA_ARGS__)
