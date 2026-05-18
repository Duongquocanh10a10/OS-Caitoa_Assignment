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
        /* TODO: put a new process to queue [q] */
		if(!q || !proc) return;
        if (q->size >= MAX_QUEUE_SIZE) return;
        q->proc[q->size] = proc;
        q->size++;
}

struct pcb_t *dequeue(struct queue_t *q)
{
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */
		int highest_idx = 0;
        struct pcb_t *ret;

        if(!q || q->size == 0) return NULL;
        for(int i =1; i < q->size; i++){
                if(q->proc[i] == NULL) continue;
                if(!(q->proc[highest_idx]) || q->proc[i]->priority < q->proc[highest_idx]->priority){
                        highest_idx = i;
                }
        }
        ret = q->proc[highest_idx];
        for(int i=highest_idx; i< q->size - 1; i++){
                q->proc[i] = q->proc[i + 1];
        }
        q->proc[q->size - 1] = NULL;
        q->size--;

        return ret;
}

struct pcb_t *purgequeue(struct queue_t *q, struct pcb_t *proc)
{
        /* TODO: remove a specific item from queue
         * */
        int idx = -1;
        struct pcb_t *ret;
        if (!q || !proc || q->size == 0) return NULL;
        for(int i = 0; i < q->size; i++){
                if(q->proc[i] == proc){
                        idx = i;
                        break;
                }
        }
        if(idx < 0) return NULL;
        ret = q->proc[idx];

        for(int i = idx; i < q->size - 1; i++){
                q->proc[i] = q->proc[i + 1];
        }
        q->proc[q->size - 1] = NULL;
        q->size--;
        return ret;
}
