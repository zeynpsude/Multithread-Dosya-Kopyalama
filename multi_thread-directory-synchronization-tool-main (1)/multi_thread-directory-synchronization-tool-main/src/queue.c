#include <limits.h>
#include "queue.h" // Tüm tanımlar ve struct buradan geliyor

void queue_init(TaskQueue *q)
{
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->shutdown = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

void queue_push(TaskQueue *q, CopyTask task)
{
    pthread_mutex_lock(&q->lock);
    while (q->count == QUEUE_SIZE && !q->shutdown)
    {
        pthread_cond_wait(&q->not_full, &q->lock);
    }
    q->tasks[q->tail] = task;
    q->tail = (q->tail + 1) % QUEUE_SIZE;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

int queue_pop(TaskQueue *q, CopyTask *task)
{
    pthread_mutex_lock(&q->lock);
    while (q->count == 0 && !q->shutdown)
    {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }
    if (q->shutdown && q->count == 0)
    {
        pthread_mutex_unlock(&q->lock);
        return 0;
    }
    *task = q->tasks[q->head];
    q->head = (q->head + 1) % QUEUE_SIZE;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return 1;
}
// Kuyruğu Başlatma
void dir_queue_init(DirQueue *q)
{
    q->head = NULL;
    q->tail = NULL;
    q->count = 0;
}

// Kuyruğa Dizin Ekleme (Push)
void dir_queue_push(DirQueue *q, const char *src, const char *dst)
{
    DirNode *new_node = (DirNode *)malloc(sizeof(DirNode));
    if (!new_node)
    {
        perror("Bellek tahsis hatasi");
        exit(EXIT_FAILURE);
    }

    strncpy(new_node->source_path, src, PATH_MAX);
    strncpy(new_node->dest_path, dst, PATH_MAX);
    new_node->next = NULL;

    if (q->tail == NULL)
    {
        q->head = new_node;
        q->tail = new_node;
    }
    else
    {
        q->tail->next = new_node;
        q->tail = new_node;
    }
    q->count++;
}

// Kuyruktan Dizin Çekme (Pop)
// Eğer kuyruk boşsa 0, başarılıysa 1 döndürür.
int dir_queue_pop(DirQueue *q, char *out_src, char *out_dst)
{
    if (q->head == NULL)
    {
        return 0; // Kuyruk boş
    }

    DirNode *temp = q->head;
    strncpy(out_src, temp->source_path, PATH_MAX);
    strncpy(out_dst, temp->dest_path, PATH_MAX);

    q->head = q->head->next;
    if (q->head == NULL)
    {
        q->tail = NULL; // Son eleman da çıktıysa kuyruğu sıfırla
    }

    free(temp); // Belleği sisteme iade et
    q->count--;

    return 1;
}

// Kuyruk Boş Mu Kontrolü
int dir_queue_is_empty(DirQueue *q)
{
    return q->head == NULL;
}
