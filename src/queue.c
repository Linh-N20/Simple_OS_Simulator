#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t *q)
{
        if (q == NULL)
                return 1;
        return (q->size == 0);
}

void enqueue(struct queue_t *q, struct pcb_t *proc)
{
        if (q == NULL || proc == NULL || q->size >= MAX_QUEUE_SIZE)
                return;
        q->proc[q->size] = proc;
        q->size++;
}

struct pcb_t *dequeue(struct queue_t *q)
{
        if (q == NULL || q->size == 0)
                return NULL;

        struct pcb_t *proc = q->proc[0];
        int i;
        for (i = 0; i < q->size - 1; i++)
                q->proc[i] = q->proc[i + 1];
        q->size--;
        return proc;
}

struct pcb_t *purgequeue(struct queue_t *q, struct pcb_t *proc)
{
        if (q == NULL || proc == NULL)
                return NULL;
        int i;
        for (i = 0; i < q->size; i++) {
                if (q->proc[i] == proc) {
                        int j;
                        for (j = i; j < q->size - 1; j++)
                                q->proc[j] = q->proc[j + 1];
                        q->size--;
                        return proc;
                }
        }
        return NULL;
}