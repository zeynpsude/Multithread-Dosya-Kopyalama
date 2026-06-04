#include <errno.h>
#include "common.h"
#include "queue.h"
#include "log.h"

#define BUFFER_SIZE 4096

static int create_temp_file(const char *dest_path, mode_t mode, char *temp_path)
{
    for (int attempt = 0; attempt < 100; attempt++)
    {
        int written = snprintf(temp_path, PATH_MAX, "%s.tmp.%ld.%lu.%d",
                               dest_path, (long)getpid(),
                               (unsigned long)pthread_self(), attempt);
        if (written < 0 || written >= PATH_MAX)
        {
            log_event("ERROR", "Gecici dosya yolu cok uzun: %s", dest_path);
            return -1;
        }

        int fd = open(temp_path, O_WRONLY | O_CREAT | O_EXCL, mode);
        if (fd >= 0)
            return fd;

        if (errno != EEXIST)
        {
            perror("Gecici dosya acilamadi");
            log_event("ERROR", "Gecici dosya acilamadi: %s", temp_path);
            return -1;
        }
    }

    log_event("ERROR", "Benzersiz gecici dosya olusturulamadi: %s", dest_path);
    return -1;
}

void *worker_thread(void *arg)
{
    TaskQueue *q = (TaskQueue *)arg;
    CopyTask task;

    while (queue_pop(q, &task))
    {
        printf("[Worker] Kopyalaniyor: %s\n", task.source_path);

        int src_fd = open(task.source_path, O_RDONLY);
        if (src_fd < 0)
        {
            perror("Kaynak dosya acilamadi");
            log_event("ERROR", "Kaynak dosya acilamadi: %s", task.source_path);
            continue;
        }

        struct stat src_stat;
        if (fstat(src_fd, &src_stat) == -1)
        {
            perror("Kaynak dosya bilgisi alinamadi");
            log_event("ERROR", "Kaynak dosya bilgisi alinamadi: %s", task.source_path);
            close(src_fd);
            continue;
        }

        char temp_path[PATH_MAX];
        int temp_fd = create_temp_file(task.dest_path, src_stat.st_mode & 0777, temp_path);
        if (temp_fd < 0)
        {
            close(src_fd);
            continue;
        }

        char buffer[BUFFER_SIZE];
        ssize_t bytes_read, bytes_written;
        long total_bytes = 0;
        int success = 1;

        while ((bytes_read = read(src_fd, buffer, BUFFER_SIZE)) > 0)
        {
            char *out_ptr = buffer;
            ssize_t bytes_to_write = bytes_read;

            while (bytes_to_write > 0)
            {
                bytes_written = write(temp_fd, out_ptr, bytes_to_write);

                if (bytes_written <= 0)
                {
                    perror("Yazma hatasi");
                    log_event("ERROR", "Yazma hatasi: %s", task.dest_path);
                    success = 0;
                    break;
                }

                bytes_to_write -= bytes_written;
                out_ptr += bytes_written;
                total_bytes += bytes_written;
            }

            if (!success)
                break;
        }

        if (bytes_read < 0)
        {
            perror("Okuma hatasi");
            log_event("ERROR", "Okuma hatasi: %s", task.source_path);
            success = 0;
        }

        if (success && fchmod(temp_fd, src_stat.st_mode & 0777) == -1)
        {
            log_event("ERROR", "Dosya izinleri gecici dosyaya uygulanamadi: %s", temp_path);
        }

        if (success && fsync(temp_fd) == -1)
        {
            perror("Gecici dosya diske yazilamadi");
            log_event("ERROR", "Gecici dosya diske yazilamadi: %s", temp_path);
            success = 0;
        }

        if (close(temp_fd) == -1)
        {
            perror("Gecici dosya kapatilamadi");
            log_event("ERROR", "Gecici dosya kapatilamadi: %s", temp_path);
            success = 0;
        }
        temp_fd = -1;

        if (close(src_fd) == -1)
        {
            log_event("ERROR", "Kaynak dosya kapatilamadi: %s", task.source_path);
        }
        src_fd = -1;

        if (success && rename(temp_path, task.dest_path) == -1)
        {
            perror("Hedef dosya atomik olarak degistirilemedi");
            log_event("ERROR", "Hedef dosya atomik olarak degistirilemedi: %s -> %s",
                      temp_path, task.dest_path);
            success = 0;
        }

        if (!success)
        {
            unlink(temp_path);
            continue;
        }

        if (task.is_update)
            log_event("UPDATE", "%s yenilendi (%ld bayt)", task.dest_path, total_bytes);
        else
            log_event("COPY", "%s -> %s (%ld bayt)",
                      task.source_path, task.dest_path, total_bytes);

        printf("[Worker] Tamamlandi: %s\n", task.dest_path);

        if (chmod(task.dest_path, src_stat.st_mode & 0777) == -1)
        {
            log_event("ERROR", "Dosya izinleri hedef dosyaya uygulanamadi: %s",
                      task.dest_path);
        }
    }
    return NULL;
}
