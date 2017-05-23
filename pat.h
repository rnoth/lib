#ifndef _pat_
#define _pat_
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

enum {
	PAT_ERR_NOMATCH  = -1,
	PAT_ERR_BADPAREN = -2,
	PAT_ERR_BADREP   = -3
};


struct pattern {
	long opts;
	struct patmatch *mat;
	size_t nmat;
	struct ins *prog;
};

struct patmatch {
	size_t off;
	size_t ext;
};

int  pat_compile(struct pattern *, char const *);
int  pat_execute(struct pattern *, char const *);
int  pat_execute_callback(struct pattern *, int (*)(char *, void *), void *);
void pat_free(struct pattern *);

#endif // _edna_pat_
