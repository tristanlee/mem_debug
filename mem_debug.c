#include "mem_debug.h"

//#define CONFIG_MEMORY_DEBUG    1    // Simple mem error check
//#define CONFIG_MEMORY_DEBUG    2    // Detail mem error check
#define CONFIG_MEMORY_DEBUG    3    // Detail mem error check in multitask environment

#define PLATFORM_SYS_RT_THREAD		1

#if defined(PLATFORM_SYS_RT_THREAD)
#include <rtthread.h>
#define PLATFORM_MALLOC(x) rt_malloc(x)
#define PLATFORM_FREE(x)   rt_free(x)
#define PLATFORM_MEMSET    rt_memset
#define PLATFORM_MEMCPY rt_memcpy
#define PLATFORM_STRLEN rt_strlen
#define PLATFORM_PRINTF rt_kprintf
#else
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define PLATFORM_MALLOC(x) malloc(x)
#define PLATFORM_FREE(x)   free(x)
#define PLATFORM_MEMSET    memset
#define PLATFORM_MEMCPY memcpy
#define PLATFORM_STRLEN strlen
#define PLATFORM_PRINTF    printf
#endif

#if (CONFIG_MEMORY_DEBUG > 0)
#define DPRINTF(...) PLATFORM_PRINTF(__VA_ARGS__)
#else
#define DPRINTF(...)
#endif

#ifndef NULL
#define NULL    ((void *)0)
#endif

void *gMemDebugMutex = NULL;

#if (CONFIG_MEMORY_DEBUG > 2)
#if defined(PLATFORM_SYS_RT_THREAD)
#include <rtthread.h>
#define MUTEX_CREATE() (void *)rt_mutex_create("M_DEBUG", RT_IPC_FLAG_FIFO)
#define MUTEX_LOCK() \
    if (gMemDebugMutex != NULL) \
        rt_mutex_take((rt_mutex_t)gMemDebugMutex, RT_WAITING_FOREVER)
#define MUTEX_RELEASE() \
    if (gMemDebugMutex != NULL) \
        rt_mutex_release((rt_mutex_t)gMemDebugMutex)

#elif defined(WIN32)
#include <Windows.h>
#define MUTEX_CREATE()    (void *)CreateMutex(NULL,0,"M_DEBUG")
#define MUTEX_LOCK() \
    if (gMemDebugMutex != NULL) \
        WaitForSingleObject((HANDLE)gMemDebugMutex, INFINITE)
#define MUTEX_RELEASE() \
    if (gMemDebugMutex != NULL) \
        ReleaseMutex((HANDLE)gMemDebugMutex)
#endif
#else
#define MUTEX_CREATE()    NULL
#define MUTEX_LOCK()
#define MUTEX_RELEASE()
#endif


#if (CONFIG_MEMORY_DEBUG > 0)

#if (CONFIG_MEMORY_DEBUG > 1)
#define HTAB_SIZE 128
#define hash(p, t) (((unsigned long)(p)>>3) & \
    (sizeof (t)/sizeof ((t)[0])-1))

static struct MemDescriptor
{
    struct MemDescriptor *link; // Hash entry link
    const void *ptr;
    long size;
    const char *file;
    int line;
} *htab[HTAB_SIZE];
#endif

void MEMD_Init(void)
{
    if (gMemDebugMutex) return;
    gMemDebugMutex = MUTEX_CREATE();
}

void* MEMD_Alloc(long nbytes, const char *file, int line)
{
    char *ptr = NULL;
#if (CONFIG_MEMORY_DEBUG > 1)
    unsigned h;
#endif

    do
    {
        //assert(nbytes > 0);
        if (nbytes <= 0)
        {
            DPRINTF("[%s:%d] MEMD_Alloc: nbytes=%d\n", file, line, (int)nbytes);
            break;
        }
#if (CONFIG_MEMORY_DEBUG > 1)
        nbytes += sizeof(struct MemDescriptor) + 1;
#endif
        if ((ptr = PLATFORM_MALLOC(nbytes)) == NULL)
        {
            // Later should raise a MEM_FAILED exception
            DPRINTF("[%s:%d] MEMD_Alloc: failed\n", file, line);
            break;
        }

#if (CONFIG_MEMORY_DEBUG > 1)
        ((struct MemDescriptor *)ptr)->ptr = ptr + sizeof(struct MemDescriptor);
        ((struct MemDescriptor *)ptr)->size = nbytes - (sizeof(struct MemDescriptor) + 1);
        ((struct MemDescriptor *)ptr)->file = file;
        ((struct MemDescriptor *)ptr)->line = line;

        MUTEX_LOCK();
        h = hash(ptr, htab);
        ((struct MemDescriptor *)ptr)->link = htab[h];
        htab[h] = (struct MemDescriptor *)ptr;

        ptr[nbytes-1] = (((long)ptr) & 0xFF) ^ 0xC5;
        ptr += sizeof(struct MemDescriptor);
        MUTEX_RELEASE();
#endif
    }
    while (0);

    return (void *)ptr;
}

void MEMD_Free(void *ptr, const char *file, int line)
{
    if (ptr)   // Better not release a null buffter, guy
    {
#if (CONFIG_MEMORY_DEBUG > 1)
        struct MemDescriptor **ap;
        ptr = (char *)ptr - sizeof(struct MemDescriptor);

        MUTEX_LOCK();
        ap = &htab[hash(ptr,htab)];
        while (*ap && (*ap) != (struct MemDescriptor *)ptr)
        {
            ap = &(*ap)->link;
        }

        if (*ap == NULL)
        {
            // Later should raise a MEM_FAILED exception
            MUTEX_RELEASE();
            DPRINTF("[%s:%d] MEMD_Free: invalid pointer\n", file, line);
            return;
        }
        // Detects storing off the end of the allocated space in
        // the buffer by comparing the end of buffer checksum with
        // the address of the buffer.
        if (((unsigned char *)ptr)[sizeof(struct MemDescriptor) + (*ap)->size] != ((((long)ptr) & 0xFF) ^ 0xC5))
        {
            DPRINTF("[%s:%d] MEMD_Free: Mem stored off, %ld bytes allocated at [%s:%d]\n",
                    file, line, (*ap)->size, (*ap)->file, (*ap)->line);
        }

        (*ap) = (*ap)->link;
        MUTEX_RELEASE();
#endif
        PLATFORM_FREE(ptr);
    }
}

void* MEMD_Strdup(const char *str, const char *file, int line)
{
    void *ptr = NULL;
    long len;

    do
    {
        if (!str)
        {
            DPRINTF("[%s:%d] MEMD_Strdup: str=NULL\n", file, line);
            break;
        }
        len = PLATFORM_STRLEN(str)+1;
        ptr = MEMD_Alloc(len, file, line);
        if (!ptr) break;
        PLATFORM_MEMCPY(ptr, str, len);
    }
    while (0);
    return ptr;
}

void MEMD_Check( void )
{
#if (CONFIG_MEMORY_DEBUG > 1)
    int i;
    struct MemDescriptor *ap;

    MUTEX_LOCK();
    for (i=0; i<HTAB_SIZE; i++)
    {
        ap = htab[i];
        while (ap)
        {
            DPRINTF("[%s:%d] MEMD_Check: %ld bytes leak at address %p\n", \
                    ap->file, ap->line, ap->size, ap->ptr);
            if (((unsigned char *)ap)[sizeof(struct MemDescriptor) + ap->size] != ((((long)ap) & 0xFF) ^ 0xC5))
            {
                DPRINTF("[%s:%d] MEMD_Check: Mem stored off, %ld bytes allocated\n", \
                        ap->file, ap->line, ap->size);
            }
            ap = ap->link;
        }
    }
    MUTEX_RELEASE();
#endif

}

void *MEMD_Calloc(long count, long nbytes, const char *file, int line)
{
    void *ptr = NULL;
    do
    {
        //assert(count > 0);
        //assert(nbytes > 0);
        if (count <= 0 || nbytes <=0)
        {
            DPRINTF("[%s:%d] MEMD_Calloc: count=%d, nbytes=%d\n", file, line, (int)count, (int)nbytes);
            break;
        }
        ptr = MEMD_Alloc(count*nbytes, file, line);
        if (!ptr) break;
        PLATFORM_MEMSET(ptr, '\0', count*nbytes);
    }
    while (0);
    return ptr;
}

void* MEMD_Realloc(void *ptr, long nbytes, const char *file, int line)
{
    void *newptr = NULL;

#if (CONFIG_MEMORY_DEBUG > 1)
    struct MemDescriptor **ap;

    do
    {
        //assert(nbytes > 0);
        if (nbytes <= 0)
        {
            DPRINTF("[%s:%d] MEMD_Realloc: nbytes=%d\n", file, line, (int)nbytes);
            break;
        }
        if (ptr == NULL)
        {
            newptr = MEMD_Alloc(nbytes, file, line);
            break;
        }

        ptr = (char *)ptr - sizeof(struct MemDescriptor);
        MUTEX_LOCK();
        ap = &htab[hash(ptr,htab)];
        while (*ap && (*ap) != (struct MemDescriptor *)ptr)
        {
            ap = &(*ap)->link;
        }
        if (*ap == NULL)
        {
            // Later should raise a MEM_FAILED exception
            MUTEX_RELEASE();
            DPRINTF("[%s:%d] MEMD_Realloc: invalid pointer\n", file, line);
            break;
        }

        ptr = (char *)ptr + sizeof(struct MemDescriptor);
        if ((*ap)->size == nbytes)
        {
            // If the old and new sizes are the same, be a nice guy
            newptr = ptr;
            MUTEX_RELEASE();
            break;
        }
        MUTEX_RELEASE();

        newptr = MEMD_Alloc(nbytes, file, line);
        if (newptr != NULL)
        {
            PLATFORM_MEMCPY(newptr, ptr, nbytes < (*ap)->size ? nbytes : (*ap)->size);
            MEMD_Free(ptr, file, line);
        }
    }
    while (0);
#endif

    return newptr;
}

#endif

