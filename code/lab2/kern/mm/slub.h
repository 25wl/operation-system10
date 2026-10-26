#ifndef __KERN_MM_SLUB_H__
#define __KERN_MM_SLUB_H__

#include <defs.h>

#define SLUB_ALIGNMENT 16
#define SLUB_SMALL_MAX 2048

/* Initialize once after PMM. Single-core, serial, non-interrupt use only. */
void slub_init(void);
/* Zero size or allocation failure returns NULL; memory is not zeroed. */
void *slub_alloc(size_t size);
/* Accepts NULL or an outstanding allocation's exact starting address. */
void slub_free(void *ptr);

#endif /* !__KERN_MM_SLUB_H__ */
