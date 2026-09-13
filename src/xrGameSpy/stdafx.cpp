#include "stdafx.h"

#if defined(__ANDROID__)
#include <pthread.h>
extern "C" int pthread_cancel(pthread_t)
{
    // Android Bionic libc does not implement pthread_cancel
    return 0;
}
#endif
