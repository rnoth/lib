#ifndef _set_
#define _set_
#include <stdbool.h>
#include <stddef.h>

typedef struct Set Set;

Set *set_alloc(void);
void set_free(Set *);

int set_add(Set *, void *, size_t);
int set_adds(Set *, char *);
int set_rm(Set *, void *, size_t);
int set_rms(Set *, char *);

bool set_memb(Set *, void *, size_t);
bool set_membs(Set *, char *);
void *set_query(Set *, void *, size_t);
void *set_querys(Set *, char *);
#endif
