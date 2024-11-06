#include <memory.h>
#include <stdlib.h>

#include "simplelist.h"

void *SimpleListAddBuffer(SimpleList *list, void *value, int size) {
  if (!list->Buffer) {
    return NULL;
  }

  // Check if the current buffer has enough space, if not, realloc
  if (list->Size + size > list->MaxSize) {
    int newMaxSize = list->MaxSize == 0 ? size : list->MaxSize * 2;
    while (newMaxSize < list->Size + size) {
      newMaxSize *= 2;
    }
    void *newBuffer = realloc(list->Buffer, newMaxSize);
    if (!newBuffer) {
      return NULL;
    }
    list->Buffer = newBuffer;
    list->MaxSize = newMaxSize;
  }

  // Append the new data to the buffer
  void *dest = (char *)list->Buffer + list->Size;
  memcpy(dest, value, size);
  list->Size += size;

  return dest;
}
