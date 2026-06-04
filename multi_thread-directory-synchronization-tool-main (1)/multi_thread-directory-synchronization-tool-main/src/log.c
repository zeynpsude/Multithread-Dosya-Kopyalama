#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include "log.h"

static FILE *log_fp = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void log_init(const char *path) {
    log_fp = fopen(path, "a");
    if (!log_fp) {
        fprintf(stderr,
            "[log] Log dosyasi acilamadi: %s. stderr'e yazilacak.\n", path);
        log_fp = stderr;
        return;
    }
    log_event("INFO", "Log baslatildi: %s", path);
}

void log_event(const char *type, const char *fmt, ...) {
    if (!log_fp) return;

    time_t t = time(NULL);
    struct tm tm_info;
    localtime_r(&t, &tm_info);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_info);

    pthread_mutex_lock(&log_mutex);

    fprintf(log_fp, "[%s] [tid:%lu] [%s] ",
            ts, (unsigned long)pthread_self(), type);

    va_list args;
    va_start(args, fmt);
    vfprintf(log_fp, fmt, args);
    va_end(args);

    fputc('\n', log_fp);
    fflush(log_fp);

    pthread_mutex_unlock(&log_mutex);
}

void log_close(void) {
    pthread_mutex_lock(&log_mutex);
    if (log_fp && log_fp != stderr) {
        time_t t = time(NULL);
        struct tm tm_info;
        localtime_r(&t, &tm_info);
        char ts[32];
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_info);
        fprintf(log_fp, "[%s] [INFO] Log kapatiliyor\n", ts);
        fclose(log_fp);
        log_fp = NULL;
    }
    pthread_mutex_unlock(&log_mutex);
}
