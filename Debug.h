#pragma once

#include <cstdio>

#define DBG(...) do { fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); } while(0)
