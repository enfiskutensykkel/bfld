#ifndef __BFLD_LOGGING_H__
#define __BFLD_LOGGING_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>


typedef struct {
    const char *name;
    const char *file;
    const char *section;
    size_t offset;
    unsigned lineno;
} log_ctx_t;


extern int __log_verbosity;

extern int __log_ctx_idx;

#define LOG_CTX_NUM 16
extern log_ctx_t __log_ctx[LOG_CTX_NUM];


#define LOG_CTX(_name, ...) ((log_ctx_t) { \
    .name = (_name) ? (_name) : __log_ctx[__log_ctx_idx].name, \
    __VA_ARGS__ \
})

#define LOG_CTX_EMPTY LOG_CTX(NULL)

#define LOG_CTX_FILE(filename, ...) LOG_CTX(NULL, .file = (filename), __VA_ARGS__)


static inline
void log_ctx_push(log_ctx_t ctx)
{
    if (__log_ctx_idx < LOG_CTX_NUM - 1) {
        __log_ctx[++__log_ctx_idx] = ctx;
    }
}


static inline
void log_ctx_pop(void)
{
    if (__log_ctx_idx > 0) {
        --__log_ctx_idx;
    }
}


#define LOG_FATAL   -1
#define LOG_ERROR   0
#define LOG_WARNING 1
#define LOG_NOTICE  2
#define LOG_INFO    3
#define LOG_DEBUG   4
#define LOG_TRACE   5


static inline
void log_message(int level, const char *fmt, ...)
{
    if (level <= __log_verbosity) {
        const log_ctx_t *ctx = &__log_ctx[__log_ctx_idx];

        if (ctx->file != NULL) {
            fprintf(stderr, "[%s", ctx->file ? ctx->file : "unknown");

            if (ctx->section != NULL) {
                fprintf(stderr, ":%s", ctx->section);
            }

            if (ctx->offset) {
                fprintf(stderr, "+0x%zx", ctx->offset);
            }

            if (ctx->lineno > 0) {
                fprintf(stderr, ":%u", ctx->lineno);
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

        va_list ap;
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);

        fprintf(stderr, "\n");
    }
}


#define log_trace(fmt, ...) \
    log_message(LOG_TRACE, fmt, ##__VA_ARGS__);


#define log_debug(fmt, ...) \
    log_message(LOG_DEBUG, fmt, ##__VA_ARGS__);


#define log_info(fmt, ...) \
    log_message(LOG_INFO, fmt, ##__VA_ARGS__);


#define log_notice(fmt, ...) \
    log_message(LOG_NOTICE, fmt, ##__VA_ARGS__);


#define log_warning(fmt, ...) \
    log_message(LOG_WARNING, fmt, ##__VA_ARGS__);


#define log_error(fmt, ...) \
    log_message(LOG_ERROR, fmt, ##__VA_ARGS__);


#define log_fatal(fmt, ...) \
    log_message(LOG_FATAL, fmt, ##__VA_ARGS__);


#ifdef __cplusplus
}
#endif
#endif
