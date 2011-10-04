#ifndef assh_shell_h
#define assh_shell_h

#include "avmshell.h"
#include "eval.h"

using namespace avmshell;

int   run_shell( int argc, char **argv );
void  run_repl();
void  parse_args( int argc, char **argv, ShellSettings settings );
void  single_worker( ShellSettings settings );
void  single_worker_helper( ShellCore *shell, ShellSettings &settings );

void  gc_init();
void  gc_end();

char *get_input();
char *get_term_prompt();
void  handle_input( char* line );
void  eval_string( char* str );

void  print_help();

#endif
