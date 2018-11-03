#ifndef _SHELL_TASKS_H
#define _SHELL_TASKS_H

#include <mrsh/ast.h>
#include "shell/task.h"

struct task *task_for_simple_command(struct mrsh_simple_command *sc);
struct task *task_for_node(struct mrsh_node *node);
struct task *task_for_command_list_array(struct mrsh_array *array);
struct task *task_for_command(struct mrsh_command *cmd);
struct task *task_for_if_clause(struct mrsh_if_clause *ic);
struct task *task_for_loop_clause(struct mrsh_loop_clause *lc);
struct task *task_for_command(struct mrsh_command *cmd);
struct task *task_for_for_loop(struct mrsh_for_clause *fc);
struct task *task_for_function_definition(struct mrsh_function_definition *fn);
struct task *task_for_pipeline(struct mrsh_pipeline *pl);
struct task *task_for_binop(struct mrsh_binop *binop);
struct task *task_for_node(struct mrsh_node *node);

#endif
