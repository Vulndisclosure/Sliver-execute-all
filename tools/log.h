#pragma once
#include <cstdio>

#ifdef _MSC_VER
#include <format>
#define __fmtns std
#else
#include <fmt/core.h>
#define __fmtns fmt
#endif

#define print(fmt, ...) \
  fputs(__fmtns::format(fmt, ##__VA_ARGS__).c_str(), stdout)
#define println(fmt, ...) puts(__fmtns::format(fmt, ##__VA_ARGS__).c_str())

#define error(fmt, ...)                                                   \
  fputs(__fmtns::format("\033[31m" fmt "\033[0m", ##__VA_ARGS__).c_str(), \
        stderr)
#define errorln(fmt, ...)                                                   \
  fputs(__fmtns::format("\033[31m" fmt "\033[0m\n", ##__VA_ARGS__).c_str(), \
        stderr)

#ifdef _DEBUG
#define trace print
#define traceln println
#else
#define trace(fmt, ...) (void)0
#define traceln(fmt, ...) (void)0
#endif
