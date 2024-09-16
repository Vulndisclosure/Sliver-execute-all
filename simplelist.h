#pragma once

typedef struct {
  void *Buffer;
  int Size;
  int MaxSize;
} SimpleList;

#define SimpleListForEach(list, type, entry, code)                             \
  {                                                                            \
    type *__p = (type *)(list)->Buffer;                                        \
    if (__p) {                                                                 \
      for (type *entry = __p; entry < (type *)((char *)__p + (list)->Size);    \
           ++entry) {                                                          \
        code;                                                                  \
      }                                                                        \
    }                                                                          \
  }
