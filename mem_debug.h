#ifndef __MEM_DEBUG_H__
#define __MEM_DEBUG_H__

#ifdef __cplusplus
extern "C" {
#endif

// Here we write our own secure malloc that will be monitoring memory heap

void* MEMD_Alloc(long nbytes, const char *file, int line);
void MEMD_Free(void *ptr, const char *file, int line);
void* MEMD_Calloc(long count, long nbytes, const char *file, int line);
void* MEMD_Realloc(void *ptr, long nbytes, const char *file, int line);
void* MEMD_Strdup(const char *str, const char *file, int line);
void MEMD_Init(void);
void MEMD_Check( void );

#ifdef __cplusplus
}
#endif

#endif // __MEM_DEBUG_H__
