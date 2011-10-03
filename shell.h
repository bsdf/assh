#ifndef assh_shell_h
#define assh_shell_h

#include "avmshell.h"
#include "eval.h"

int         run_shell( int argc, char **argv );
void        run_repl();
void        parse_args( int argc, char **argv, avmshell::ShellSettings settings );

char       *get_input();
const char *get_term_prompt();
void        handle_input( char* line );
void		eval_string( char* str );

void        print_help();

#endif
