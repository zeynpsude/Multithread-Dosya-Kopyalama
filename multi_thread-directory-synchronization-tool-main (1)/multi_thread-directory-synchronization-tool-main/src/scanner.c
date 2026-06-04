#include <errno.h>
#include <limits.h>
#include "queue.h"
#include "log.h"

static int files_differ(const char *src_path, const char *dest_path)
{
    int src_fd = open(src_path, O_RDONLY);
    if (src_fd < 0)
    {
        log_event("ERROR", "Karsilastirma icin kaynak dosya acilamadi: %s", src_path);
        return 0;
    }

    int dest_fd = open(dest_path, O_RDONLY);
    if (dest_fd < 0)
    {
        log_event("ERROR", "Karsilastirma icin hedef dosya acilamadi: %s", dest_path);
        close(src_fd);
        return 1;
    }

    char src_buf[4096];
    char dest_buf[4096];
    int different = 0;

    while (1)
    {
        ssize_t src_read = read(src_fd, src_buf, sizeof(src_buf));
        ssize_t dest_read = read(dest_fd, dest_buf, sizeof(dest_buf));

        if (src_read < 0 || dest_read < 0)
        {
            log_event("ERROR", "Dosya karsilastirma okuma hatasi: %s / %s",
                      src_path, dest_path);
            different = 0;
            break;
        }

        if (src_read != dest_read)
        {
            different = 1;
            break;
        }

        if (src_read == 0)
            break;

        if (memcmp(src_buf, dest_buf, (size_t)src_read) != 0)
        {
            different = 1;
            break;
        }
    }

    close(src_fd);
    close(dest_fd);
    return different;
}

void scan_directory(const char *root_src, const char *root_dst, TaskQueue *task_q)
{
    DirQueue dq;
    dir_queue_init(&dq);
    dir_queue_push(&dq, root_src, root_dst);

    char current_src[PATH_MAX];
    char current_dst[PATH_MAX];

    while (dir_queue_pop(&dq, current_src, current_dst))
    {
        DIR *dir = opendir(current_src);
        if (!dir)
        {
            perror("Dizin acilamadi");
            log_event("ERROR", "Dizin acilamadi: %s", current_src);
            continue;
        }

        struct dirent *entry;
        struct stat src_stat, dest_stat;

        while ((entry = readdir(dir)) != NULL)
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            char src_path[PATH_MAX];
            char dest_path[PATH_MAX];

            if (snprintf(src_path, PATH_MAX, "%s/%s", current_src, entry->d_name) >= PATH_MAX)
            {
                log_event("ERROR", "Kaynak yolu cok uzun: %s/%s", current_src, entry->d_name);
                continue;
            }
            if (snprintf(dest_path, PATH_MAX, "%s/%s", current_dst, entry->d_name) >= PATH_MAX)
            {
                log_event("ERROR", "Hedef yolu cok uzun: %s/%s", current_dst, entry->d_name);
                continue;
            }

            if (lstat(src_path, &src_stat) == -1)
            {
                log_event("ERROR", "Kaynak dosya bilgisi alinamadi: %s", src_path);
                continue;
            }

            if (S_ISLNK(src_stat.st_mode))
                continue;

            if (S_ISDIR(src_stat.st_mode))
            {
                if (mkdir(dest_path, src_stat.st_mode & 0777) == -1)
                {
                    if (errno != EEXIST)
                    {
                        perror("Hedef alt dizin olusturulamadi");
                        log_event("ERROR", "Hedef alt dizin olusturulamadi: %s", dest_path);
                        continue;
                    }

                    if (stat(dest_path, &dest_stat) == -1 || !S_ISDIR(dest_stat.st_mode))
                    {
                        log_event("ERROR", "Hedef yolu dizin degil, alt agac atlandi: %s",
                                  dest_path);
                        continue;
                    }
                }

                dir_queue_push(&dq, src_path, dest_path);
            }
            else if (S_ISREG(src_stat.st_mode))
            {
                int need_copy = 0;
                int is_update = 0;

                if (stat(dest_path, &dest_stat) == -1)
                {
                    if (errno == ENOENT)
                    {
                        need_copy = 1;
                        is_update = 0;
                    }
                    else
                    {
                        log_event("ERROR", "Hedef dosya bilgisi alinamadi: %s", dest_path);
                        continue;
                    }
                }
                else if (src_stat.st_mtime > dest_stat.st_mtime ||
                         src_stat.st_size != dest_stat.st_size)
                {
                    need_copy = 1;
                    is_update = 1;
                }
                else if (files_differ(src_path, dest_path))
                {
                    need_copy = 1;
                    is_update = 1;
                }

                if (need_copy)
                {
                    CopyTask task;
                    strncpy(task.source_path, src_path, PATH_MAX - 1);
                    task.source_path[PATH_MAX - 1] = '\0';
                    strncpy(task.dest_path, dest_path, PATH_MAX - 1);
                    task.dest_path[PATH_MAX - 1] = '\0';
                    task.is_update = is_update;

                    printf("[Scanner] Kuyruga alindi (%s): %s\n",
                           is_update ? "UPDATE" : "COPY", src_path);
                    queue_push(task_q, task);
                }
            }
        }
        closedir(dir);
    }
}
