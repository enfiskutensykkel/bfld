#include "cdefs.h"
#include "logging.h"


int log_level = 3;

const char *log_phase = NULL;

_Thread_local int log_ctx = 0;

_Thread_local log_ctx_t log_ctx_stack[LOG_CTX_MAX];


