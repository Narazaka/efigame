#include <Uefi.h>

#ifndef MATH_H
#define MATH_H

int abs(int n) {
  return n >= 0 ? n : -n;
}

float pow(float n, int p) {
  float result = 1;
  while (p--) {
    result *= n;
  }
  return result;
}

#endif
