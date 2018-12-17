#ifndef MRSH_GETOPT_H
#define MRSH_GETOPT_H

extern char *mrsh_optarg;
extern int mrsh_opterr, mrsh_optind, mrsh_optopt;

int mrsh_getopt(int argc, char * const argv[], const char *optstring);

#endif
