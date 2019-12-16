#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#define SIDE_LEFT	0
#define SIDE_RGHT	1

#define TYPE_END	0
#define TYPE_PIPE	1
#define TYPE_BREAK	2

typedef struct	s_list
{
	char			**args;
	int				length;
	int				type;
	int				pipes[2];
	struct s_list	*previous;
	struct s_list	*next;
}				t_list;

int
	ft_strlen(char const *str)
{
	int	i;

	i = 0;
	while (str[i])
		i++;
	return (i);
}

int
	show_error(char const *str)
{
	if (str)
		write(STDERR_FILENO, str, ft_strlen(str));
	return (1);
}

int
	exit_fatal(void)
{
	show_error("error: fatal\n");
	exit(1);
	return (1);
}

char
	*ft_strdup(char const *str)
{
	char	*copy;
	int		i;

	if (!(copy = (char*)malloc(sizeof(*copy) * ft_strlen(str))))
	{
		exit_fatal();
		return (NULL);
	}
	i = 0;
	while (str[i])
	{
		copy[i] = str[i];
		i++;
	}
	copy[i] = 0;
	return (copy);
}

int
	add_arg(t_list *cmd, char *arg)
{
	char	**tmp;
	int		i;

	i = 0;
	tmp = NULL;
	if (cmd->length > 0)
	{
		if (!(tmp = (char**)malloc(sizeof(*tmp) * (cmd->length + 2))))
			return (exit_fatal());
		while (i < cmd->length)
		{
			tmp[i] = cmd->args[i];
			i++;
		}
		free(cmd->args);
		cmd->args = tmp;
	}
	else
	{
		if (!(tmp = (char**)malloc(sizeof(*tmp) * 2)))
			return (exit_fatal());
		cmd->args = tmp;
	}
	if (!(cmd->args[i++] = ft_strdup(arg)))
		return (exit_fatal());
	cmd->length++;
	cmd->args[i] = 0;
	return (0);
}

int
	list_push(t_list **list, char *arg)
{
	t_list	*new;

	if (!(new = (t_list*)malloc(sizeof(*new))))
		return (exit_fatal());
	new->args = NULL;
	new->length = 0;
	new->type = TYPE_END;
	new->previous = NULL;
	new->next = NULL;
	if (*list)
	{
		(*list)->next = new;
		new->previous = *list;
	}
	*list = new;
	return (add_arg(new, arg));
}

int
	list_rewind(t_list **list)
{
	while ((*list)->previous)
		*list = (*list)->previous;
	return (0);
}

int
	args_clear(t_list *cmd)
{
	int	i;

	i = 0;
	while (i < cmd->length)
		free(cmd->args[i++]);
	free(cmd->args);
	cmd->args = NULL;
	cmd->length = 0;
	return (0);
}

int
	list_clear(t_list **cmds)
{
	t_list	*tmp;

	list_rewind(cmds);
	while (*cmds)
	{
		tmp = (*cmds)->next;
		if ((*cmds)->length > 0)
			args_clear(*cmds);
		free(*cmds);
		*cmds = tmp;
	}
	*cmds = NULL;
	return (0);
}

int
	parse_arg(t_list **cmds, char *arg)
{
	if ((!*cmds || (*cmds)->type > TYPE_END))
		return (list_push(cmds, arg));
	else
	{
		if (strcmp("|", arg) == 0)
			(*cmds)->type = TYPE_PIPE;
		else if (strcmp(";", arg) == 0)
			(*cmds)->type = TYPE_BREAK;
		else
			return (add_arg(*cmds, arg));
	}
	return (0);
}

int
	exec_cmd(t_list *cmd, char **args, char * const *env)
{
	pid_t	pid;
	int		ret;
	int		status;

	if (cmd->type == TYPE_PIPE)
	{
		if (pipe(cmd->pipes) < 0)
			return (exit_fatal());
	}
	pid = fork();
	if (pid == 0)
	{
		if (cmd->type == TYPE_PIPE)
		{
			if (dup2(cmd->pipes[SIDE_RGHT], STDOUT_FILENO) < 0)
				return (exit_fatal());
		}
		if (cmd->previous
			&& cmd->previous->type == TYPE_PIPE
			&& dup2(cmd->previous->pipes[SIDE_LEFT], STDIN_FILENO) < 0)
			return (exit_fatal());
		ret = execve(args[0], args, env);
		if (ret < 0)
		{
			show_error("error: cannot execute ");
			show_error(args[0]);
			show_error("\n");
		}
		if (cmd->type == TYPE_PIPE)
			close(cmd->pipes[SIDE_RGHT]);
		if (cmd->previous
			&& cmd->previous->type == TYPE_PIPE)
			close(cmd->pipes[SIDE_LEFT]);
		exit(ret);
		return (ret);
	}
	else if (pid < 0)
		return (exit_fatal());
	else
	{
		waitpid(pid, &status, 0);
		if (cmd->type == TYPE_PIPE)
			close(cmd->pipes[SIDE_RGHT]);
		if (cmd->type == TYPE_PIPE
			&& cmd->previous
			&& cmd->previous->type == TYPE_PIPE)
			close(cmd->previous->pipes[SIDE_LEFT]);
		if (WIFEXITED(status))
			return (WEXITSTATUS(status));
		return (1);
	}
	return (0);
}

int
	exec_cmds(t_list **cmds, char * const *env)
{
	t_list	*crt;

	env = NULL;
	list_rewind(cmds);
	while (*cmds)
	{
		crt = *cmds;
		if (strcmp("cd", crt->args[0]) == 0)
		{
			if (crt->length != 2)
				return (show_error("error: cd: bad arguments\n"));
			if (chdir(crt->args[1]) != 0)
			{
				return (show_error("error: cd: cannot change directory to ")
					&& show_error(crt->args[1])
					&& show_error("\n"));
			}
		}
		else if (exec_cmd(crt, crt->args, env))
		{
			while ((*cmds)->next && (*cmds)->type == TYPE_PIPE)
				*cmds = (*cmds)->next;
		}
		if (!(*cmds)->next)
			break ;
		*cmds = (*cmds)->next;
	}
	return (0);
}

int
	exit_error(t_list **cmds, char const *str)
{
	if (cmds)
		list_clear(cmds);
	if (str)
		show_error(str);
	return (1);
}

int
	main(int argc, char **argv, char * const *env)
{
	t_list	*cmds;
	int		i;

	if (argc < 2)
	{
		write(STDOUT_FILENO, "\n", 1);
		return (0);
	}
	cmds = NULL;
	i = 1;
	while (i < argc)
		if (parse_arg(&cmds, argv[i++]))
			return (exit_fatal());
	if (exec_cmds(&cmds, env))
		return (list_clear(&cmds) | 1);
	list_clear(&cmds);
	return (0);
}