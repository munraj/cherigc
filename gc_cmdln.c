#include <stdio.h>
#include <string.h>

#include "gc.h"
#include "gc_cmdln.h"
#include "gc_debug.h"

struct gc_cmd	gc_cmds[] = {
	{.c_cmd = (const char *[]){"help", "h", NULL}, .c_fn = &gc_cmd_help,
	 .c_desc = "Display help"},
	{.c_cmd = (const char *[]){"cont", "c", "", NULL}, .c_fn = &gc_cmd_cont,
	 .c_desc = "Continue running"},
	{.c_cmd = (const char *[]){"next", "n", NULL}, .c_fn = &gc_cmd_next,
	 .c_desc = "Step one \"logical\" step"},
	{.c_cmd = (const char *[]){"stat", "s", NULL}, .c_fn = &gc_cmd_stat,
	 .c_desc = "Display statistics"},
	{.c_cmd = (const char *[]){"map", "m", NULL}, .c_fn = &gc_cmd_map,
	 .c_desc = "Display btbl map"},
	{.c_cmd = NULL},
};

int
gc_cmd_help(struct gc_cmd *cmd, char **arg)
{
	const char **namep;

	for (cmd = gc_cmds; cmd->c_cmd != NULL; cmd++) {
		for (namep = cmd->c_cmd; *namep != NULL; namep++)
			printf("%s ", *namep);
		printf("- %s\n", cmd->c_desc);
	}
	return (0);
}

int
gc_cmd_cont(struct gc_cmd *cmd, char **arg)
{

	return (1);
}

int
gc_cmd_next(struct gc_cmd *cmd, char **arg)
{

	gc_state_c->gs_enter_cmdln_on_log = 1;
	return (1);
}

int
gc_cmd_stat(struct gc_cmd *cmd, char **arg)
{
	int i;

	gc_print_siginfo_status();
	for (i = 0; i < GC_LOG_BIGSZ; i++)
		printf("ntalloc %d = %zu\n", 1 << i, gc_state_c->gs_ntalloc[i]);
	printf("ntbigalloc = %zu\n", gc_state_c->gs_ntbigalloc);
	return (0);
}

int
gc_cmd_map(struct gc_cmd *cmd, char **arg)
{
	_gc_cap struct gc_btbl *btbl;
	int error;

	error = 0;
	if (arg[1] == NULL)
		error = 1;
	else if (strcmp(arg[1], "b") == 0)
		btbl = &gc_state_c->gs_btbl_big;
	else if (strcmp(arg[1], "s") == 0)
		btbl = &gc_state_c->gs_btbl_small;
	else
		error = 1;

	if (error)
		printf("map: b (big) or s (small)\n");
	else
		gc_print_map(btbl);

	return (0);
}

void
gc_cmdin(char *buf, size_t bufsz)
{
	int i;

	printf(">> ");
	for (i = 0; i < bufsz - 1; i++) {
		buf[i] = getchar();
		if (buf[i] == '\n')
			break;
	}
	buf[i] = '\0';
}

void
gc_cmdarg(char *buf, char **arg, size_t narg)
{
	int i;

	for (i = 0; i < narg - 1; i++) {
		arg[i] = buf;
		while (*buf != '\0' && *buf != ' ')
			buf++;
		if (*buf == '\0') {
			i++;
			break;
		}
		*buf = '\0';
		buf++;
	}
	arg[i] = NULL;
}

int
gc_cmdrn(char **arg)
{
	struct gc_cmd *cmd;
	const char **namep;
	int rc;

	rc = 0;
	for (cmd = gc_cmds; cmd->c_cmd != NULL; cmd++) {
		for (namep = cmd->c_cmd; *namep != NULL; namep++) {
			if (strcmp(*namep, arg[0]) == 0) {
				rc = cmd->c_fn(cmd, arg);
				goto done;
			}
		}
	}
	if (cmd->c_cmd == NULL) {
		printf("unrecognized: `%s'\n", arg[0]);
	}
done:
	return (rc);
}

void
gc_cmdln(void)
{
	char buf[512];
	char *arg[10];
	int rc;

	/* Disable stepping until explicitly requested again. */
	gc_state_c->gs_enter_cmdln_on_log = 0;

	for (;;) {
		gc_cmdin(buf, sizeof(buf));
		gc_cmdarg(buf, arg, sizeof(arg)/sizeof(*arg));
		rc = gc_cmdrn(arg);
		if (rc)
			break;
	}
}
