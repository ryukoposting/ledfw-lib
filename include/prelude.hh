#pragma once

#define __STDC_WANT_LIB_EXT1__ 1
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <algorithm>

#include "nordic_common.h"
#include "nrf.h"
#include "app_error.h"
#include "FreeRTOS.h"
#include "task.h"
#include "nrf_log.h"

#ifndef MANUFACTURER_NAME
#define MANUFACTURER_NAME "NOMFG"
#endif

#ifndef unused
#define unused(X) ((void)X)
#endif

#ifndef stringify
#define stringify_(X) # X
#define stringify(X) stringify_(X)
#endif

#ifndef concat
#define concat_(X,Y) X ## Y
#define concat(X,Y) concat_(X,Y)
#endif

#ifndef concat3
#define concat3_(X,Y,Z) X ## Y ## Z
#define concat3(X,Y,Z) concat3_(X,Y,Z)
#endif

#ifndef assert
#define assert(expr) do {\
        if (!(expr)) {\
            APP_ERROR_HANDLER(ERROR_ASSERT);\
        }\
    } while (0)
#endif

#ifndef dassert
#ifdef NDEBUG
#define dassert(expr) ((void)expr)
#else
#define dassert(expr) do {\
        if (!(expr)) {\
            APP_ERROR_HANDLER(ERROR_ASSERT);\
        }\
    } while (0)
#endif /* !NDEBUG */
#endif /* dassert */

#ifndef unreachable
#define unreachable() do {\
        APP_ERROR_HANDLER(ERROR_UNREACHABLE);\
    } while (1)
#endif

#ifndef todo
#define todo() do {\
        APP_ERROR_HANDLER(ERROR_UNIMPLEMENTED);\
    } while (1)
#endif

#ifdef __GNUC__
#ifndef packed_struct
#define packed_struct struct __attribute__ ((packed))
#endif /* packed_struct */
#else
#error 'packed_struct' attribute not implemented for this compiler.
#endif

#ifdef __GNUC__
#ifndef align
#define align(X) __attribute__((aligned(X)))
#endif /* aligned(X) */
#else
#error 'aligned(X)' attribute not implemented for this compiler.
#endif

#include "def/errors.def"
