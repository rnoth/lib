#include <errno.h>
#include <string.h>
#include <util.h>
#include <pat.ih>

int
thr_alloc(struct thread *thr[static 2])
{
	struct thread *ret;

	ret = calloc(1, sizeof *ret);
	if (!ret) return ENOMEM;

	thr[0] = ret;
	thr[1] = ret;

	return 0;
}

int
thr_cmp(struct thread *lt, struct thread *rt)
{
	size_t min;
	ptrdiff_t cmp;

	if (!lt && !rt) return 0;
	if (!lt) return -1;
	if (!rt) return  1;

	min = umin(lt->nmat, rt->nmat);

	iterate(i, min) {
		cmp = ucmp(lt->mat[i].off, rt->mat[i].off);
		if (cmp) return -cmp;

		cmp = ucmp(lt->mat[i].ext, rt->mat[i].ext);
		if (cmp) return cmp;
	}

	return 0;
}

int
thr_init(struct thread *th, struct ins *prog)
{
	th->ip = prog;

	th->nmat = 0;

	return 0;
}

int
thr_fork(struct thread *dst, struct thread *src)
{
	dst->ip = src->ip;
	dst->nmat = src->nmat;
	memcpy(dst->mat, src->mat, sizeof dst->mat);

	return dst->mat ? 0 : ENOMEM;
}

void
thr_free(struct thread *th)
{
	struct thread *a;

	while (th) a = th, th = a->next, free(a);
}

void
thr_mv(struct thread *dst[static 2], struct thread **src)
{
	struct thread *tmp;
	tmp = src[0];
	src[0] = src[0]->next;
	tmp->next = 0;
	if (dst[1]) dst[1]->next = tmp;
	else dst[0] = tmp;
	dst[1] = tmp;
}
