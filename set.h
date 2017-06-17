#ifndef _set_
#define _set_
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct set;

struct set *set_alloc(void);
void   set_free(struct set *);

int    set_add      (struct set *, uint8_t *, size_t);
bool   set_contains (struct set *, uint8_t *, size_t);
bool   set_prefix   (struct set *, uint8_t *, size_t);
size_t set_query    (void ***, size_t, struct set *, uint8_t *, size_t);
int    set_remove   (struct set *, uint8_t *, size_t);

inline static
int   set_add_string      (struct set *t, char *s){return set_add      (t,(void *)s,strlen(s)+1);}
inline static
bool  set_contains_string (struct set *t, char *s){return set_contains (t,(void *)s,strlen(s)+1);}
inline static
bool  set_prefix_string   (struct set *t, char *s){return set_prefix   (t,(void *)s,strlen(s));}
inline static
int   set_remove_string   (struct set *t, char *s){return set_remove   (t,(void *)s,strlen(s)+1);}

inline static
int   set_add_bytes      (struct set *t, void *y, size_t n) { return set_add      (t, y, n); }
inline static
bool  set_contains_bytes (struct set *t, void *y, size_t n) { return set_contains (t, y, n); }
inline static
bool  set_prefix_bytes   (struct set *t, void *y, size_t n) { return set_prefix   (t, y, n); }
inline static
int   set_remove_bytes   (struct set *t, void *y, size_t n) { return set_remove   (t, y, n); }

inline static
size_t
set_query_string(void *out, size_t nmemb, struct set *t, char *s)
{ return set_query(out, nmemb, t, (void *)s, strlen(s)); }

inline static
size_t
set_query_bytes(void *out, size_t nmemb, struct set *t, void *y, size_t n)
{ return set_query(out, nmemb, t, y, n); }

#endif
