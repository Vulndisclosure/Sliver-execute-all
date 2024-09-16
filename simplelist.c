#include "simplelist.h"
#include <stdlib.h>

int SimpleListAlloc(SimpleList *list, int initialSize) {
  list->Buffer = malloc(initialSize);
  if (list->Buffer) {
    list->Size = 0;
    list->MaxSize = initialSize;
    return initialSize;
  } else {
    list->Size = 0;
    list->MaxSize = 0;
    return 0;
  }
}

void SimpleListFree(SimpleList *list) {
  if (list->Buffer) {
    free(list->Buffer);
    list->Buffer = NULL;
  }
  list->Size = 0;
  list->MaxSize = 0;
}

void *SimpleListAddBuffer(SimpleList *list, void *value, int size) {
  if (!list->Buffer)
    return NULL;

  // Check if the current buffer has enough space, if not, realloc
  if (list->Size + size > list->MaxSize) {
    int newMaxSize = list->MaxSize == 0 ? size : list->MaxSize * 2;
    while (newMaxSize < list->Size + size)
      newMaxSize *= 2;
    void *newBuffer = realloc(list->Buffer, newMaxSize);
    if (!newBuffer)
      return NULL;
    list->Buffer = newBuffer;
    list->MaxSize = newMaxSize;
  }

  // Append the new data to the buffer
  void *dest = (char *)list->Buffer + list->Size;
  _memcpy(dest, value, size);
  list->Size += size;

  return dest;
}

#define SimpleListAdd(list, type, value)                                       \
  do {                                                                         \
    type __t = (value);                                                        \
    SimpleListAddBuffer((list), &__t, sizeof(__t));                            \
  } while (0)
