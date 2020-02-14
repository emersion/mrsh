#ifndef MRSH_GETOPT_H
#define MRSH_GETOPT_H

extern char *_mrsh_optarg;
extern int _mrsh_opterr, _mrsh_optind, _mrsh_optopt;

int _mrsh_getopt(int argc, char * const argv[], const char *optstring);

#endif
