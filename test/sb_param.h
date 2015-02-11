#ifndef _SB_PARAM_H_
#define _SB_PARAM_H_

struct sb_param
{
	struct cheri_object	sp_gc;	/* garbage collector object */
	int			sp_op;	/* operation */
	__capability void *__capability *sp_cap1;	/* place in which to store a cap */
};

#define	OP_INIT		0
#define	OP_TRY_USE	1

#endif /* !_SB_PARAM_H_ */
