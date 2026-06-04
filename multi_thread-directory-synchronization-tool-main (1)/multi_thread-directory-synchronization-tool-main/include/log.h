#ifndef LOG_H
#define LOG_H

/*
 * Thread-safe loglama modulu.
 * Tum dosya kopyalama, guncelleme ve hata olaylarini
 * timestamp + thread id ile log dosyasina yazar.
 */

void log_init(const char *path);
void log_event(const char *type, const char *fmt, ...);
void log_close(void);

#endif
