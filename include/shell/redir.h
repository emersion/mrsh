#ifndef SHELL_REDIR_H
#define SHELL_REDIR_H

#include <mrsh/ast.h>

int process_redir(const struct mrsh_io_redirect *redir, int *redir_fd);

#endif
