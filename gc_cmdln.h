#ifndef _GC_CMDLN_H_
#define _GC_CMDLN_H_

#include <stdlib.h>

struct gc_cmd;

typedef int		gc_cmd_fn(struct gc_cmd *_cmd, char **_arg);

struct gc_cmd {
	const char	**c_cmd;	/* command name(s) */
	gc_cmd_fn	*c_fn;		/* function to invoke */
};

extern struct gc_cmd	gc_cmds[];

void		gc_cmdarg(char *_buf, char **_arg, size_t _narg);
void		gc_cmdin(char *_buf, size_t _bufsz);
void		gc_cmdln(void);
int		gc_cmdrn(char **_arg);

/* Commands. */
gc_cmd_fn	gc_cmd_help;
gc_cmd_fn	gc_cmd_cont;

#endif /* !_GC_CMDLN_H_ */
