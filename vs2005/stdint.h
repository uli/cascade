

#ifndef _MSC_STDINT_H_ // [
#define _MSC_STDINT_H_


#pragma once

#include <limits.h>

// For Visual Studio 6 in C++ mode and for many Visual Studio versions when
// compiling for ARM we should wrap <wchar.h> include with 'extern "C++" {}'
// or compiler give many errors like this:
//   error C2733: second C linkage of overloaded function 'wmemchr' not allowed
#ifdef __cplusplus
extern "C" {
#endif
#  include <wchar.h>
#ifdef __cplusplus
}
#endif

// Define _W64 macros to mark types changing their size, like intptr_t.
#ifndef _W64
#  if !defined(__midl) && (defined(_X86_) || defined(_M_IX86)) 
#     define _W64 __w64
#  else
#     define _W64
#  endif
#endif


   typedef signed __int8     int8_t;
   typedef signed __int16    int16_t;
   typedef signed __int32    int32_t;
   typedef unsigned __int8   uint8_t;
   typedef unsigned __int16  uint16_t;
   typedef unsigned __int32  uint32_t;

typedef signed __int64       int64_t;
typedef unsigned __int64     uint64_t;

#ifdef _WIN64 // [
   typedef signed __int64    intptr_t;
   typedef unsigned __int64  uintptr_t;
#else // _WIN64 ][
   typedef _W64 signed int   intptr_t;
   typedef _W64 unsigned int uintptr_t;
#endif // _WIN64 ]


#endif // _MSC_STDINT_H_ ]
