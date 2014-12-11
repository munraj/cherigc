#include <stdio.h>
#include <string.h>

#include "gc_cmdln.h"

struct gc_cmd	gc_cmds[] = {
	{.c_cmd = (const char *[]){"help", "h", NULL}, .c_fn = &gc_cmd_help},
	{.c_cmd = (const char *[]){"cont", "c", NULL}, .c_fn = &gc_cmd_cont},
	{.c_cmd = NULL},
};

int
gc_cmd_help(struct gc_cmd *cmd, char **arg)
{
	const char **namep;

	for (cmd = gc_cmds; cmd->c_cmd != NULL; cmd++) {
		for (namep = cmd->c_cmd; *namep != NULL; namep++)
			printf("%s ", *namep);
		printf("\n");
	}
	return (0);
}

int
gc_cmd_cont(struct gc_cmd *cmd, char **arg)
{

	return (1);
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
		rc = 0;
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

	for (;;) {
		gc_cmdin(buf, sizeof(buf));
		gc_cmdarg(buf, arg, sizeof(arg)/sizeof(*arg));
		rc = gc_cmdrn(arg);
		if (rc)
			break;
	}
}
