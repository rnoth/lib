#ifndef _set_
#define _set_
#include <stdbool.h>
#include <stddef.h>

#include "vec.h"

typedef struct internal Set;


Set *set_alloc(void);
void set_free(Set *);

int    set_add      (Set *, void *, size_t);
int    set_remove   (Set *, void *, size_t);
bool   set_contains (Set *, void *, size_t);
bool   set_prefix   (Set *, void *, size_t);
void * set_query    (Set *, void *, size_t);

inline static int   set_add_string      (Set *t, char *s){return set_add      (t,(void *)s,strlen(s)+1);}
inline static bool  set_contains_string (Set *t, char *s){return set_contains (t,(void *)s,strlen(s)+1);}
inline static bool  set_prefix_string   (Set *t, char *s){return set_prefix   (t,(void *)s,strlen(s));}
inline static void *set_query_string    (Set *t, char *s){return set_query    (t,(void *)s,strlen(s));}
inline static int   set_remove_string   (Set *t, char *s){return set_remove   (t,(void *)s,strlen(s)+1);}

inline static int   set_add_bytes      (Set *t, void *y, size_t n) { return set_add      (t, y, n); }
inline static bool  set_contains_bytes (Set *t, void *y, size_t n) { return set_contains (t, y, n); }
inline static bool  set_prefix_bytes   (Set *t, void *y, size_t n) { return set_prefix   (t, y, n); }
inline static void *set_query_bytes    (Set *t, void *y, size_t n) { return set_query    (t, y, n); }
inline static int   set_remove_bytes   (Set *t, void *y, size_t n) { return set_remove   (t, y, n); }

inline static int   set_add_vector      (Set *t, void *v) { return set_add      (t, v, vec_len(v)); }
inline static bool  set_contains_vector (Set *t, void *v) { return set_contains (t, v, vec_len(v)); }
inline static bool  set_prefix_vector   (Set *t, void *v) { return set_prefix   (t, v, vec_len(v)); }
inline static void *set_query_vector    (Set *t, void *v) { return set_query    (t, v, vec_len(v)); }
inline static int   set_remove_vector   (Set *t, void *v) { return set_remove   (t, v, vec_len(v)); }
#endif
