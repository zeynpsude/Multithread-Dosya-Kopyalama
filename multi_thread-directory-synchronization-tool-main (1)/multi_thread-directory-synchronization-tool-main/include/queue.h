#ifndef QUEUE_H
#define QUEUE_H

#include "common.h"

#define QUEUE_SIZE 512

// Dosya kopyalama kuyruğu
typedef struct
{
    CopyTask tasks[QUEUE_SIZE];
    int head, tail, count, shutdown;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} TaskQueue;

// Gezilecek dizin kuyruğu
typedef struct
{
    DirNode *head;
    DirNode *tail;
    int count; // Kuyruktaki dizin sayısını takip etmek için (isteğe bağlı ama faydalı)
} DirQueue;

void queue_init(TaskQueue *q);
void queue_push(TaskQueue *q, CopyTask task);
int queue_pop(TaskQueue *q, CopyTask *task);

void dir_queue_init(DirQueue *q);
void dir_queue_push(DirQueue *q, const char *src, const char *dst);
int dir_queue_pop(DirQueue *q, char *out_src, char *out_dst);
int dir_queue_is_empty(DirQueue *q);
#endif
