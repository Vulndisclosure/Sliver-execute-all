#pragma once

typedef struct {
  void *Buffer;
  int Size;
  int MaxSize;
} SimpleList;

int SimpleListAlloc(SimpleList *list, int initialSize);
void SimpleListFree(SimpleList *list);
void *SimpleListAddBuffer(SimpleList *list, void *value, int size);

#define SimpleListForEach(list, type, entry, code)                          \
  {                                                                         \
    type *__p = (type *)(list)->Buffer;                                     \
    if (__p) {                                                              \
      for (type *entry = __p; entry < (type *)((char *)__p + (list)->Size); \
           ++entry) {                                                       \
        code;                                                               \
      }                                                                     \
    }                                                                       \
  }

#define SimpleListAdd(list, type, value)            \
  do {                                              \
    type __t = (value);                             \
    SimpleListAddBuffer((list), &__t, sizeof(__t)); \
  } while (0)
