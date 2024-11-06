#include <stdlib.h>

#include "simplelist.h"

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
