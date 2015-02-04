#ifndef _SB_PARAM_H_
#define _SB_PARAM_H_

struct sb_param
{
	struct cheri_object	sp_gc;	/* garbage collector object */
	int			sp_op;	/* operation */
};

#define	OP_INIT		0
#define	OP_TRY_USE	1

#endif /* !_SB_PARAM_H_ */
