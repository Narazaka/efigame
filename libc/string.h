#include <Uefi.h>

#ifndef STRING_H
#define STRING_H

void* memset(void *buf, int val, UINTN size) {
  unsigned char *tmp = (unsigned char *)buf;
  while (size--) *tmp++ = val;
  return buf;
}

void *memcpy(void *dest, const void *src, UINTN n) {
  unsigned char *d = (unsigned char *)dest;
  unsigned char const *s = (unsigned char const *)src;
  while (n--) *d++ = *s++;
  return dest;
}

#endif
