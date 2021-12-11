
#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
#include <assert.h>
#include "green.h"

#define FALSE 0
#define TRUE 1

#define STACK_SIZE 4096

static ucontext_t main_cntx = {0};
static green_t main_green = {&main_cntx, NULL, NULL, NULL, NULL, NULL, TRUE};
green_t *running = &main_green;

static queue_t *ready_q = &(queue_t) {
    .first = NULL,
    .last = NULL
};


static void init() __attribute__((constructor));

void init() {
    getcontext(&main_cntx);
}

void printlength() {
    green_t *current = ready_q->first;
    int size = 0;
    while (current != NULL) {
        size++;
        current = current->next;
    }
    printf("\t\tlength___: %d\n", size);
}

void insert(green_t *new, queue_t *queue) {
    if (ready_q->first == NULL) {
        ready_q->first = new;
        ready_q->last = new;
        return;
    } else {
        new->next = NULL;
        queue->last->next = new;
        queue->last = new;
    }
}

green_t *detach(queue_t *queue) {
    green_t *use = queue->first;
    if (use == NULL) {
        fprintf(stderr, "queue null\n");
        exit(EXIT_FAILURE);
    }                    //2+ threads in queue + sentinel
    queue->first = use->next;
    return use;
}

void green_cond_init(green_cond_t *cond) {
    cond->queue = malloc(sizeof(green_cond_t));
    cond->queue->first = NULL;
    cond->queue->last = NULL;
}

void green_cond_wait(green_cond_t *cond) {

    
}

void green_cond_signal(green_cond_t *cond) {
    
}

void green_thread() {
    green_t *this = running;

    void *result = (*this->fun)(this->arg);

    if (this->join != NULL) {
        insert(this->join, ready_q); //place waiting (joining) thread in ready queu
    }
    
    this->retval = result;      //save result of execution
    this->zombie = TRUE;        //zombie state

    green_t *next = detach(ready_q); //find the next thread to run
    running = next;
    setcontext(next->context);
}

int green_create(green_t *new, void *(*fun)(void*), void *arg) {
    ucontext_t *cntx = (ucontext_t*)malloc(sizeof(ucontext_t));
    getcontext(cntx);

    void *stack = malloc(STACK_SIZE);
    cntx->uc_stack.ss_sp = stack;
    cntx->uc_stack.ss_size = STACK_SIZE;
    makecontext(cntx, green_thread, 0);

    new->context = cntx;
    new->fun = fun;
    new->arg = arg;
    new->next = NULL;
    new->join = NULL;
    new->retval = NULL;
    new->zombie = FALSE;
    insert(new, ready_q);
    return 0;
}

int green_yield() {
    green_t *susp = running;
    insert(susp, ready_q);               //set susp last in queue

    green_t *next = detach(ready_q); //get first in queue
    running = next;
    swapcontext(susp->context, next->context);
    return 0;
}

int green_join(green_t *thread, void **res) {
    if (!thread->zombie) {
        green_t *susp = running;
        thread->join = susp;
        
        green_t *next = detach(ready_q);
        running = next;
        swapcontext(susp->context, next->context);
    }
    if (res != NULL) {
        *res = thread->retval;
    }
    free(thread->context);
    return 0;
}