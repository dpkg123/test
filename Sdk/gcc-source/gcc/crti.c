#include "config.h"

typedef void (*func_ptr) (void);

_init(int argc, char **argv, char **environ)
{
	DO_GLOBAL_CTORS_BODY;
}
_fini()
{
	DO_GLOBAL_DTORS_BODY;
}
