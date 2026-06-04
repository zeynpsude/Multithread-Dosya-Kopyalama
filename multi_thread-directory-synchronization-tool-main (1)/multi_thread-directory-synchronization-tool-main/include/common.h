#ifndef COMMON_H
#define COMMON_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <limits.h>
#include <fcntl.h>

// Her bir dizin yapısını temsil eden yapı
typedef struct DirNode
{
    char source_path[PATH_MAX];
    char dest_path[PATH_MAX];
    struct DirNode *next;
} DirNode;

// Her bir kopyalama işini temsil eden yapı
typedef struct
{
    char source_path[PATH_MAX];
    char dest_path[PATH_MAX];
    int is_update; // 0 = yeni dosya (COPY), 1 = değişmiş dosya (UPDATE)
} CopyTask;

#endif