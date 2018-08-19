#ifndef __MEM_DEBUG_H__
#define __MEM_DEBUG_H__

//#define CONFIG_MEMORY_DEBUG    1    // Simple mem error check
//#define CONFIG_MEMORY_DEBUG    2    // Detail mem error check
#define CONFIG_MEMORY_DEBUG    3    // Detail mem error check in multitask environment

#define PLATFORM_SYS_RT_THREAD		1

#if defined(PLATFORM_SYS_ABSTRACT_LAYER)
#include "sys_al/sys_al_ex.h"
#define PLATFORM_MALLOC(x) SYS_Malloc(x)
#define PLATFORM_FREE(x)   SYS_Free(x)
#define PLATFORM_REALLOC(x, y)    LIBC_Realloc(x, y)
#define PLATFORM_CALLOC(x, y)    LIBC_Calloc(x, y)
#define PLATFORM_STRDUP(x)	LIBC_Strdup(x)
#else

#if defined(PLATFORM_SYS_RT_THREAD)
#include <rtthread.h>
#define PLATFORM_MALLOC(x) rt_malloc(x)
#define PLATFORM_FREE(x)   rt_free(x)
#define PLATFORM_REALLOC(x, y)    rt_realloc(x, y)
#define PLATFORM_CALLOC(x, y)    rt_calloc(x, y)
#define PLATFORM_STRDUP(x) rt_strdup(x)
#else
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define PLATFORM_MALLOC(x) malloc(x)
#define PLATFORM_FREE(x)   free(x)
#define PLATFORM_REALLOC(x, y)    realloc(x, y)
#define PLATFORM_CALLOC(x, y)    calloc(x, y)
#define PLATFORM_STRDUP(x) strdup(x)
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

// Here we write our own secure malloc that will be monitoring memory heap
#if (CONFIG_MEMORY_DEBUG > 0)

#define MALLOC(nbytes) \
    MEMD_Alloc((nbytes), __FILE__, __LINE__)
#define NEW(p)    ((p) = MALLOC((long)sizeof *(p)))
#define FREE(ptr)    ((void)(MEMD_Free((ptr), \
    __FILE__, __LINE__), (ptr) = 0))
#define CALLOC(count, nbytes) \
    MEMD_Calloc((count), (nbytes), __FILE__, __LINE__)
#define NEW0(p)    ((p) = CALLOC(1, (long)sizeof *(p)))
#define REALLOC(ptr, nbytes)    MEMD_Realloc((ptr), \
    (nbytes), __FILE__, __LINE__)
#define STRDUP(str) \
    MEMD_Strdup((str), __FILE__, __LINE__)
#define MEMDEBUG_INIT()    MEMD_Init()
#define MEMDEBUG_CHECK()    MEMD_Check()

void* MEMD_Alloc(long nbytes, const char *file, int line);
void MEMD_Free(void *ptr, const char *file, int line);
void* MEMD_Calloc(long count, long nbytes, const char *file, int line);
void* MEMD_Realloc(void *ptr, long nbytes, const char *file, int line);
void* MEMD_Strdup(const char *str, const char *file, int line);
void MEMD_Init(void);
void MEMD_Check( void );

#else

#define MALLOC(x) PLATFORM_MALLOC(x)
#define NEW(p)    ((p) = PLATFORM_MALLOC(sizeof *(p)))
#define FREE(x)    PLATFORM_FREE(x)
#define CALLOC(x, y) PLATFORM_CALLOC(x, y)
#define NEW0(p)    ((p) = PLATFORM_CALLOC(1, sizeof *(p)))
#define REALLOC(x, y) PLATFORM_REALLOC(x, y)
#define STRDUP(x)   PLATFORM_STRDUP(x)
#define MEMDEBUG_INIT()    ((void)0)
#define MEMDEBUG_CHECK() ((void)0)

#endif // CONFIG_MEMORY_DEBUG

#ifdef __cplusplus
}
#endif

#endif // __MEM_DEBUG_H__
