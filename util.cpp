#include "util.h"
#include <string.h>

int eq( char *a, char *b ) {
	if ( strcmp( a, b ) == 0)
		return 1;
	else
		return 0;
}
