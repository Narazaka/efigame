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

CHAR16* strncpy(CHAR16 *dest, const CHAR16 *src, UINTN n) {
  CHAR16 *tmp_dest = dest;
  const CHAR16 *tmp_src = src;
  while(n--) *tmp_dest++ = *tmp_src++;
  return dest;
}

CHAR16* strcpy(CHAR16 *dest, const CHAR16 *src) {
  CHAR16 *tmp_dest = dest;
  const CHAR16 *tmp_src = src;
  while(TRUE) {
    *tmp_dest = *tmp_src;
    if (*tmp_src == L'\0') break;
    ++tmp_dest;
    ++tmp_src;
  }
  return dest;
}

CHAR16* strcat(CHAR16 *dest, const CHAR16 *src) {
  CHAR16 *tmp_dest = dest;
  const CHAR16 *tmp_src = src;
  while(*tmp_dest != L'\0') ++tmp_dest;
  strcpy(tmp_dest, src);
  return dest;
}

UINTN strlen(CHAR16 *str) {
  UINTN len = 0;
  while(*(str + len) != L'\0') ++len;
  return len;
}

#endif
