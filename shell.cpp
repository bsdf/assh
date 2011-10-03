#include <stdio.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "shell.h"
#include "util.h"

using namespace avmplus;

int run_shell( int argc, char **argv ) {
	run_repl();
	return 0;
}

static int repl_should_run = 1;
void run_repl() {
	char* line;
	
	while( repl_should_run )
	{
		line = get_input();
		
		if ( line ) {
			handle_input( line );
		}
		else return;
	}
}

static char *line_read = (char *)NULL;
char *get_input() {
	if ( line_read ) {
		free( line_read );
		line_read = (char *)NULL;
	}
	
	line_read = readline( get_term_prompt() );
	
	if ( line_read && *line_read ) {
		add_history( line_read );
	}
	
	return line_read;
}

const char *get_term_prompt() {
	return "~assh> ";
}

void handle_input(char* line) {
	if ( eq( line, "?" ) ) {
		print_help();
	}
	else if ( eq( line, ".quit" ) ) {
		repl_should_run = 0;
	}
	else {
		eval_string( line );
	}
}

void eval_string( char* str ) {
	AvmCore      *core;
	Toplevel     *top_level;
	String       *code;
	String       *filename;
	ScriptBuffer  buffer = avmplus::compileProgram( core, top_level, code, filename);
	
//	String* input;
//	input = avmplus::String::createLatin1( NULL, str, 10 );
//	ScriptBuffer buffer = avmplus::compileProgram( NULL, NULL, input, NULL );
	
//	AvmplusHostContext context(core, toplevel);
//	StUTF16String src(code);
//	StUTF16String fn(filename);
//	AvmAssert(src.c_str()[src.length()-1] == 0);
//	RTC::Compiler compiler(&context, filename == NULL ? NULL : fn.c_str(), src.c_str(), src.length(), true);
//	TRY(core, kCatchAction_Rethrow) {
//		compiler.compile();
//		return context.script_buffer;
//	}
//	CATCH(Exception *exception) {
//		compiler.destroy();
//		core->throwException(exception);
//		/*NOTREACHED*/
//	again:
//		goto again;
//	}
//	END_CATCH
//	END_TRY
}

void print_help() {
	printf( "HELP.\n\n" );
}

avmshell::Platform* avmshell::Platform::GetInstance() {
	return (avmshell::Platform *)NULL;
}
