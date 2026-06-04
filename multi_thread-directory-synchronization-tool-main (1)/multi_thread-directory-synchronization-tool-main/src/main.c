#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include "queue.h"
#include "scanner.h"
#include "worker.h"
#include "log.h"

TaskQueue file_queue;

static int is_same_or_child_path(const char *parent, const char *child)
{
    size_t parent_len = strlen(parent);

    if (strcmp(parent, child) == 0)
        return 1;

    if (strcmp(parent, "/") == 0)
        return child[0] == '/';

    return strncmp(parent, child, parent_len) == 0 &&
           child[parent_len] == '/';
}

static int resolve_missing_dest_path(const char *dest_dir, char *resolved_dest)
{
    char path_copy[PATH_MAX];
    char parent_path[PATH_MAX];
    char real_parent[PATH_MAX];
    char *slash;
    char *base;
    size_t len;

    if (snprintf(path_copy, PATH_MAX, "%s", dest_dir) >= PATH_MAX)
    {
        fprintf(stderr, "Hata: hedef yolu cok uzun: %s\n", dest_dir);
        log_event("ERROR", "Hedef yolu cok uzun: %s", dest_dir);
        return -1;
    }

    len = strlen(path_copy);
    while (len > 1 && path_copy[len - 1] == '/')
    {
        path_copy[len - 1] = '\0';
        len--;
    }

    slash = strrchr(path_copy, '/');
    if (!slash)
    {
        snprintf(parent_path, PATH_MAX, ".");
        base = path_copy;
    }
    else if (slash == path_copy)
    {
        snprintf(parent_path, PATH_MAX, "/");
        base = slash + 1;
    }
    else
    {
        *slash = '\0';
        snprintf(parent_path, PATH_MAX, "%s", path_copy);
        base = slash + 1;
    }

    if (*base == '\0')
    {
        fprintf(stderr, "Hata: hedef dizin adi gecersiz: %s\n", dest_dir);
        log_event("ERROR", "Hedef dizin adi gecersiz: %s", dest_dir);
        return -1;
    }

    if (!realpath(parent_path, real_parent))
    {
        perror("Hedef ust dizini cozumlenemedi");
        log_event("ERROR", "Hedef ust dizini cozumlenemedi: %s", parent_path);
        return -1;
    }

    if (strcmp(real_parent, "/") == 0)
    {
        if (snprintf(resolved_dest, PATH_MAX, "/%s", base) >= PATH_MAX)
        {
            log_event("ERROR", "Hedef yolu cok uzun: %s", dest_dir);
            return -1;
        }
    }
    else if (snprintf(resolved_dest, PATH_MAX, "%s/%s", real_parent, base) >= PATH_MAX)
    {
        log_event("ERROR", "Hedef yolu cok uzun: %s", dest_dir);
        return -1;
    }

    return 0;
}

static int prepare_directories(const char *src_dir, const char *dest_dir,
                               char *real_src, char *real_dest)
{
    struct stat src_stat, dest_stat;

    if (!realpath(src_dir, real_src))
    {
        perror("Kaynak dizin cozumlenemedi");
        log_event("ERROR", "Kaynak dizin cozumlenemedi: %s", src_dir);
        return -1;
    }

    if (stat(real_src, &src_stat) == -1 || !S_ISDIR(src_stat.st_mode))
    {
        fprintf(stderr, "Hata: kaynak yolu gecerli bir dizin degil: %s\n", src_dir);
        log_event("ERROR", "Kaynak yolu gecerli bir dizin degil: %s", src_dir);
        return -1;
    }

    if (stat(dest_dir, &dest_stat) == -1)
    {
        if (errno != ENOENT)
        {
            perror("Hedef dizin kontrol edilemedi");
            log_event("ERROR", "Hedef dizin kontrol edilemedi: %s", dest_dir);
            return -1;
        }

        if (resolve_missing_dest_path(dest_dir, real_dest) == -1)
            return -1;

        if (is_same_or_child_path(real_src, real_dest))
        {
            fprintf(stderr, "Hata: hedef dizin kaynak dizinin kendisi veya icinde olamaz.\n");
            log_event("ERROR", "Hedef dizin kaynak dizinin kendisi veya icinde olamaz: src=%s dst=%s",
                      real_src, real_dest);
            return -1;
        }

        if (mkdir(dest_dir, 0755) == -1)
        {
            perror("Hedef dizin olusturulamadi");
            log_event("ERROR", "Hedef dizin olusturulamadi: %s", dest_dir);
            return -1;
        }
    }
    else if (!S_ISDIR(dest_stat.st_mode))
    {
        fprintf(stderr, "Hata: hedef yolu bir dizin degil: %s\n", dest_dir);
        log_event("ERROR", "Hedef yolu bir dizin degil: %s", dest_dir);
        return -1;
    }
    else if (!realpath(dest_dir, real_dest))
    {
        perror("Hedef dizin cozumlenemedi");
        log_event("ERROR", "Hedef dizin cozumlenemedi: %s", dest_dir);
        return -1;
    }

    if (is_same_or_child_path(real_src, real_dest))
    {
        fprintf(stderr, "Hata: hedef dizin kaynak dizinin kendisi veya icinde olamaz.\n");
        log_event("ERROR", "Hedef dizin kaynak dizinin kendisi veya icinde olamaz: src=%s dst=%s",
                  real_src, real_dest);
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        fprintf(stderr, "Kullanim: %s <thread_sayisi> <kaynak_dizin> <hedef_dizin>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int thread_count = atoi(argv[1]);
    if (thread_count <= 0)
    {
        fprintf(stderr, "Hata: thread_sayisi pozitif bir tamsayi olmali.\n");
        return EXIT_FAILURE;
    }

    const char *src_dir = argv[2];
    const char *dest_dir = argv[3];
    char real_src[PATH_MAX];
    char real_dest[PATH_MAX];

    log_init("copy_tool.log");
    log_event("INFO", "Arac baslatildi: src=%s dst=%s thread_sayisi=%d",
              src_dir, dest_dir, thread_count);

    if (prepare_directories(src_dir, dest_dir, real_src, real_dest) == -1)
    {
        log_close();
        return EXIT_FAILURE;
    }

    queue_init(&file_queue);

    pthread_t *workers = malloc(thread_count * sizeof(pthread_t));
    if (!workers)
    {
        perror("Bellek tahsis hatasi");
        return EXIT_FAILURE;
    }

    for (int i = 0; i < thread_count; i++)
    {
        if (pthread_create(&workers[i], NULL, worker_thread, &file_queue) != 0)
        {
            perror("Worker thread olusturulamadi");
            log_event("ERROR", "Worker thread %d olusturulamadi", i);
            free(workers);
            return EXIT_FAILURE;
        }
    }

    printf("--- Tarama Basliyor (%d worker thread) ---\n", thread_count);

    scan_directory(real_src, real_dest, &file_queue);

    pthread_mutex_lock(&file_queue.lock);
    file_queue.shutdown = 1;
    pthread_cond_broadcast(&file_queue.not_empty);
    pthread_mutex_unlock(&file_queue.lock);

    for (int i = 0; i < thread_count; i++)
    {
        pthread_join(workers[i], NULL);
    }

    pthread_mutex_destroy(&file_queue.lock);
    pthread_cond_destroy(&file_queue.not_empty);
    pthread_cond_destroy(&file_queue.not_full);

    free(workers);

    printf("--- Tum Islemler Basariyla Tamamlandi ---\n");
    log_event("INFO", "Tum islemler tamamlandi");
    log_close();

    return 0;
}
