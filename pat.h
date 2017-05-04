#ifndef _edna_pat_
#define _edna_pat_
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

struct pattern {
	long opts;
	struct patmatch *mat;
	size_t nmat;
	struct patins *prog;
};

struct patmatch {
	size_t off;
	size_t ext;
};

int  patcomp(struct pattern *, char const *);
int  patexec(struct patmatch **, char const *, struct pattern const *);
void patfree(struct pattern *);

#endif // _edna_pat_
