#include <errno.h>
#include <util.h>
#include <vec.h>
#include <pat.ih>

int
thr_cmp(struct thread *lt, struct thread *rt)
{
	size_t min = 0;
	ptrdiff_t cmp = 0;

	if (!lt->mat && !rt->mat) return 0;
	if (!lt->mat) return -1;
	if (!rt->mat) return  1;

	min = umin(vec_len(lt->mat), vec_len(rt->mat));

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

	th->mat = vec_new(struct thread);
	if (!th->mat) return ENOMEM;;

	return 0;
}

void
thr_finish(struct context *ctx, size_t ind)
{
	if (ctx->fin->mat) vec_free(ctx->fin->mat);
	memcpy(ctx->fin, ctx->thr + ind, sizeof *ctx->fin);

	vec_delete(&ctx->thr, ind);
}

int
thr_fork(struct thread *dst, struct thread *src)
{
	dst->ip = src->ip;
	dst->mat = vec_clone(src->mat);

	return dst->mat ? 0 : ENOMEM;
}

int
thr_next(struct context *ctx, size_t ind, wchar_t wc)
{
	if (!vec_len(ctx->thr)) return 0;
	if (ind >= vec_len(ctx->thr)) return 0;

	if (ctx->fin->mat) thr_prune(ctx, ind);

	if (ind >= vec_len(ctx->thr)) return 0;
	return ctx->thr[ind].ip->op(ctx, ctx->thr + ind, wc);
}

void
thr_prune(struct context *ctx, size_t ind)
{
	size_t i;

	for (i = ind; i < vec_len(ctx->thr);) {
		if (thr_cmp(ctx->fin, ctx->thr + i) > 0) thr_remove(ctx, i);
		else return;
	}
}

int
thr_start(struct context *ctx, wchar_t wc)
{
	return thr_next(ctx, 0, wc);
}

void
thr_remove(struct context *ctx, size_t ind)
{
	vec_free(ctx->thr[ind].mat);
	vec_delete(&ctx->thr, ind);
}

