#ifndef BFLD_LOGGING_H
#define BFLD_LOGGING_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cdefs.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

#define LOG_CTX_MAX 32


/*
 * Log context structure.
 */
typedef struct {
    const char *file;
    const char *name;
    uint64_t offset;
    const char *phase;
} log_ctx_t;


extern int log_level;

extern const char *log_phase;

extern _Thread_local int log_ctx;

extern _Thread_local log_ctx_t log_ctx_stack[LOG_CTX_MAX];


static inline
void log_ctx_pop(void)
{
    if (log_ctx > 0) {
        --log_ctx;
    }
}


static inline
void log_ctx_unwind(int n)
{
    while (n-- > 0) {
        log_ctx_pop();
    }
}


/*
 * Add information to the current log context.
 * Note: Log contexts should be pushed and popped within the same function.
 */
static inline
int log_ctx_push(log_ctx_t ctx)
{
    if (log_ctx >= 0 && log_ctx < LOG_CTX_MAX - 1) {
        // Copy information from the previous context
        const log_ctx_t* current = &log_ctx_stack[log_ctx];
        if (ctx.file == NULL) {
            if (ctx.name == NULL) {
                if (ctx.offset == 0) {
                    ctx.offset = current->offset;
                }
                ctx.name = current->name;
            }
            ctx.file = current->file;
        }

        if (ctx.phase == NULL) {
            ctx.phase = current->phase;
        }

        // Create new context
        log_ctx_t new_ctx = {
            .file = ctx.file != NULL ? ctx.file : NULL,
            .name = ctx.name != NULL ? ctx.name : NULL,
            .offset = ctx.offset,
            .phase = ctx.phase != NULL ? ctx.phase : NULL
        };

        log_ctx_stack[log_ctx + 1] = new_ctx;
    }
    return ++log_ctx;
}


/*
 * Create a new log context.
 * This overrides the existing log context.
 * Note: Log contexts should be pushed and popped within the same function.
 */
static inline
int log_ctx_new(const char *file)
{
    log_ctx_t new_ctx = {
        .file = file,
    };

    if (log_ctx >= 0 && log_ctx < LOG_CTX_MAX - 1) {
        log_ctx_stack[log_ctx + 1] = new_ctx;
    }

    return ++log_ctx;
}


#define LOG_CTX(...) ((log_ctx_t) { \
    __VA_ARGS__ \
})

#define LOG_CTX_ABSOLUTE_OFFSET(_offset) LOG_CTX(.name="", .offset=(_offset))
#define LOG_CTX_SUBPHASE(_phase) LOG_CTX(.phase=(_phase))
#define LOG_CTX_SECTION(_section) LOG_CTX(.name=(_section))
#define LOG_CTX_SECTION_OFFSET(_section, _offset) LOG_CTX(.name=(_section), .offset=(_offset))
#define LOG_CTX_SYMBOL(_symbol) LOG_CTX(.name=(_symbol))
#define LOG_CTX_SYMBOL_OFFSET(_symbol, _offset) LOG_CTX(.name=(_symbol), .offset=(_offset))
#define LOG_CTX_RELATIVE_OFFSET(_offset) LOG_CTX(.offset=(_offset))


#define LOG_FATAL   -1
#define LOG_ERROR   0
#define LOG_WARNING 1
#define LOG_NOTICE  2
#define LOG_INFO    3
#define LOG_DEBUG   4
#define LOG_TRACE   5


static inline
void log_message_va(int level, const char *fmt, va_list ap)
{
    if (level <= log_level) {
        const log_ctx_t *ctx;
        int log_ctx_safe = log_ctx;

        if (log_ctx_safe >= LOG_CTX_MAX - 1) {
            log_ctx_safe = LOG_CTX_MAX - 1;
        }

        ctx = &log_ctx_stack[log_ctx_safe];

        if (ctx->file != NULL && ctx->file[0] != '\0') {
            fprintf(stderr, "[%s", ctx->file);

            if (ctx->name != NULL && ctx->name[0] != '\0') {
                fprintf(stderr, ":%s", ctx->name);
                if (ctx->offset > 0) {
                    fprintf(stderr, "+0x%zx", ctx->offset);
                }
            } else if (ctx->offset > 0) {
                fprintf(stderr, " @ 0x%zx", ctx->offset);
            }

            fprintf(stderr, "] ");

        } else if (ctx->name != NULL && ctx->name[0] != '\0') {
            fprintf(stderr, "[%s", ctx->name);
            if (ctx->offset > 0) {
                fprintf(stderr, "+0x%zx", ctx->offset);
            }
            fprintf(stderr, "] ");
        } 

        if (level <= LOG_FATAL) {
            fprintf(stderr, "fatal: ");
        } else if (level == LOG_ERROR) {
            fprintf(stderr, "error: ");
        } else if (level == LOG_WARNING) {
            fprintf(stderr, "warning: ");
        } else if (level == LOG_NOTICE) {
            fprintf(stderr, "notice: ");
        } else if (level == LOG_INFO) {
            fprintf(stderr, "info: ");
        } else if (level == LOG_DEBUG) {
            fprintf(stderr, "debug: ");
        } else {
            fprintf(stderr, "trace: ");
        }

        vfprintf(stderr, fmt, ap);
        fprintf(stderr, "\n");
    }
}


static inline
void log_message(int level, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_message_va(level, fmt, ap);
    va_end(ap);
}


static inline
void log_trace(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_message_va(LOG_TRACE, fmt, ap);
    va_end(ap);
}


static inline
void log_debug(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_message_va(LOG_DEBUG, fmt, ap);
    va_end(ap);
}


static inline
void log_info(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_message_va(LOG_INFO, fmt, ap);
    va_end(ap);
}


static inline
void log_notice(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_message_va(LOG_NOTICE, fmt, ap);
    va_end(ap);
}


static inline
void log_warning(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_message_va(LOG_WARNING, fmt, ap);
    va_end(ap);
}


static inline
void log_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_message_va(LOG_ERROR, fmt, ap);
    va_end(ap);
}


static inline
void log_fatal(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_message_va(LOG_FATAL, fmt, ap);
    va_end(ap);
}


#ifdef __cplusplus
}
#endif
#endif
