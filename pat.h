#ifndef _edna_pat_
#define _edna_pat_
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "str.h"

typedef struct pattern pattern_t;
typedef struct patmatch patmatch_t;

struct pattern {
	long opts;
	patmatch_t *mat;
	struct patins *prog;
};

struct patmatch {
	size_t off;
	size_t ext;
};

int patcomp(pattern_t *, string_t const *);
int patexec(patmatch_t **, char const *, pattern_t const *);
void patfree(pattern_t *);
int pateval(patmatch_t **, string_t const *, char const *, long);

#endif
