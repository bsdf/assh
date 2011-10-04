#include "shell.h"

void hello()
{
	printf( "\n"
		    " ╔═║╔═╝╔═╝║ ║╔═╝║  ║ \n"
		    " ╔═║══║══║╔═║╔═╝║  ║ \n"
		    " ╝ ╝══╝══╝╝ ╝══╝══╝══\n"
		    "   actionscript.shell\n"
		    "           ? for help\n\n");
}

int main (int argc, char **argv)
{
	hello();
	int code = run_shell(argc, argv);
	return code;
}
