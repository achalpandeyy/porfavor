#ifndef PORFAVOR_TYPES_H
#define PORFAVOR_TYPES_H

#include <stdint.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int32_t s32;

typedef u32 b32;

typedef float f32;
typedef double f64;

#define ArrayCount(arr) (sizeof(arr)/sizeof(arr[0]))

#endif // PORFAVOR_TYPES_H
