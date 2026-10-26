#ifndef __KERN_MM_SLUB_TEST_H__
#define __KERN_MM_SLUB_TEST_H__

/* Run after slub_init(), with no other users and at least 16 free pages. */
void slub_check(void);

#endif
