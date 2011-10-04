#include <stdio.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "shell.h"
#include "util.h"
#include "shellcore.h"

using namespace avmplus;
using namespace avmshell;

avmshell::ShellCore* repl_core;

int run_shell( int argc, char **argv ) {
	gc_init();
	
    {
        MMGC_ENTER_RETURN(OUT_OF_MEMORY);
        
        ShellSettings settings;
        parse_args( argc, argv, settings );
        single_worker(settings);
    }
	
	gc_end();
    return 0;
}

void parse_args( int argc, char **argv, ShellSettings settings ) {
    
}

void gc_init() {
	MMgc::GCHeap::EnterLockInit();
    MMgc::GCHeapConfig conf;
    MMgc::GCHeap::Init(conf);
	MMgc::GCHeap::EnterLockInit();
}

void gc_end() {
	MMgc::GCHeap::Destroy();
    MMgc::GCHeap::EnterLockDestroy();
}

void single_worker( ShellSettings settings )
{
	MMgc::GCConfig gcconfig;
    if (settings.gcthreshold != 0)
        // (zero means use default value already in gcconfig.)
        gcconfig.collectionThreshold = settings.gcthreshold;
    gcconfig.exactTracing = settings.exactgc;
    gcconfig.markstackAllowance = settings.markstackAllowance;
    gcconfig.drc = settings.drc;
    gcconfig.mode = settings.gcMode();
    gcconfig.validateDRC = settings.drcValidation;
    MMgc::GC *gc = mmfx_new( MMgc::GC(MMgc::GCHeap::GetGCHeap(), gcconfig) );
    {
        MMGC_GCENTER(gc);
        repl_core = new ShellCoreImpl( gc, settings, true );
        single_worker_helper( repl_core, settings );
        delete repl_core;
    }
    mmfx_delete( gc );
}

void single_worker_helper( ShellCore *shell, ShellSettings &settings )
{
	if (!shell->setup(settings))
        exit(1);
    
#ifdef AVMSHELL_PROJECTOR_SUPPORT
    if (settings.do_projector) {
        AvmAssert(settings.programFilename != NULL);
        int exitCode = shell->executeProjector(settings.programFilename);
        if (exitCode != 0)
            exit(exitCode);
    }
#endif
    
#ifdef VMCFG_AOT
    int exitCode = shell->evaluateFile(settings, NULL);
    if (exitCode != 0)
        exit(exitCode);
    return;
#endif
    
    // For -testswf we must have exactly one file
    if (settings.do_testSWFHasAS3 && settings.numfiles != 1)
        exit(1);
    
    // execute each abc file
    for (int i=0 ; i < settings.numfiles ; i++ ) {
        int exitCode = shell->evaluateFile(settings, settings.filenames[i]);
        if (exitCode != 0)
            exit(exitCode);
    }
    
    if (settings.do_repl)
        run_repl();
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

char *get_term_prompt() {
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
    avmplus::String* input;
    input = repl_core->newStringLatin1( str );
    repl_core->evaluateString( input, false );
}

void print_help() {
	printf( "HELP.\n\n" );
}

avmshell::Platform* avmshell::Platform::GetInstance() {
	return (avmshell::Platform *)NULL;
}

void avmshell::ConsoleOutputStream::write(const char* utf8)
{
	printf( "%s", utf8 );
}

void avmshell::ConsoleOutputStream::writeN(const char* utf8, size_t count)
{
	printf( "%s", utf8 );
}

