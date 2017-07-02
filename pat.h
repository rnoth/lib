#ifndef _lib_pat_
#define _lib_pat_
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdint.h>

enum {
	PAT_ERR_NOMATCH  = -1,
	PAT_ERR_BADPAREN = -2,
	PAT_ERR_BADREP   = -3
};

struct patmatch {
	size_t off;
	size_t ext;
};

struct pattern {
	size_t nmat;
	struct patmatch  mat[10];
	struct ins      *prog;
};

int  pat_compile(struct pattern *, char const *);
int  pat_execute(struct pattern *, char const *);
void pat_free(struct pattern *);

#endif // _lib_pat_
