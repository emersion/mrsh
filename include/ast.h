#ifndef _AST_H
#define _AST_H

#include <mrsh/ast.h>

void command_list_array_finish(struct mrsh_array *cmds);
void case_item_destroy(struct mrsh_case_item *item);

#endif
