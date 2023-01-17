#pragma once

#include "Platform.h"

#if defined(BUILD_DEBUG) || defined(BUILD_DEVELOPER) || defined(BUILD_TEST)
#define Assert(x)                                                             \
    do                                                                        \
    {                                                                         \
        if (!(x))                                                             \
            TriggerBreakpoint();                                              \
    } while (0)
#else
#define DebugTriggerbreakpoint()
#define Assert(x) \
    do            \
    {             \
        0;        \
    } while (0)
#endif

//
//
//

#define ArrFmt              "{ %zd, %p }"
#define ArrArg(x)           ((x).count), ((x).data)
#define ArrSizeInBytes(arr) ((arr).count * sizeof(*((arr).data)))
#define StrFmt              "%.*s"
#define StrArg(x)           (int)((x).count), ((x).data)
#define Str(x)              (String){ sizeof(x)-1, x }

typedef struct String {
	imem count;
	u8 * data;
} String;
