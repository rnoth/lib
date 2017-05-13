#ifndef _pat_
#define _pat_
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

int  pat_compile(struct pattern *, char const *);
int  pat_match(struct patmatch **, char const *, struct pattern const *);
int  pat_match_callback(struct patmatch **, struct pattern const *,
                        int (*)(char *, void *), void *);
void pat_free(struct pattern *);

#endif // _edna_pat_
