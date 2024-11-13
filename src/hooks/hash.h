#pragma once
#include <stdint.h>

constexpr unsigned __shift = ([]() constexpr -> unsigned {
  unsigned shifts[] = {1, 3, 5, 7, 11, 13, 17, 19, 23, 29};
  return shifts[__TIME__[7] - '0'];
})();

template <size_t N, class CharT>
constexpr unsigned __hash(const CharT (&s)[N]) {
  unsigned h = 0;
  for (size_t i = 0; i < N - 1; i++) {
    h = (h >> __shift) | (h << (32 - __shift));
    h += s[i];
  }
  return h;
}

template <unsigned v>
constexpr unsigned __force_compile_time_result() {
  return v;
}

#define HashCT(s) __force_compile_time_result<__hash(s)>()

template <class CharT>
__forceinline unsigned HashRT(const CharT *s) {
  unsigned h = 0;
  do {
    h = (h >> __shift) | (h << (32 - __shift));
    h += *s;
  } while (*++s);
  return h;
}
