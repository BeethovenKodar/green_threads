
#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
#include <assert.h>
#include "green.h"

#define FALSE 0
#define TRUE 1

#define STACK_SIZE 4096

static ucontext_t main_cntx = {0};
static green_t main_green = {&main_cntx, NULL, NULL, NULL, NULL, NULL, FALSE};
static green_t *running = &main_green;

static green_t sentinel = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRUE};
static green_t *queue = &sentinel;  //the ready queue

static void init() __attribute__((constructor));

void init() {
    getcontext(&main_cntx);
}

/* insert new thread last in queue */
void insert(green_t *new) {
    new->prev = NULL;
    new->next = queue;  //set next to previous last
    queue->prev = new;  //set prevous last prev to new last
    queue = new;        //update queue pointer
}

/* pull out thread first in queue */
green_t *retrieve() {
    
    if (sentinel.prev == NULL) { //sentinel alone in queue
        return NULL;
    }

    green_t *use = sentinel.prev;
    
    if (use->prev != NULL) { //sentinel and 2+ threads in queue
        use->prev->next = &sentinel;
        sentinel.prev = use->prev;
    } else {                //sentinel and 1 thread in queue
        sentinel.prev = NULL;
        queue = &sentinel;
    }

    return use;
}

void green_thread() {
    green_t *this = running;

    void *result = (*this->fun)(this->arg);

    if (this->join != NULL) {
        insert(this->join); //place waiting (joining) thread in ready queu
    }
    
    this->retval = result;      //save result of execution
    this->zombie = TRUE;        //zombie state

    green_t *next = retrieve(); //find the next thread to run

    if (next != NULL) {
        running = next;
        setcontext(next->context);
    }
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
    new->prev = NULL;
    new->next = NULL;
    new->join = NULL;
    new->retval = NULL;
    new->zombie = FALSE;

    insert(new);

    return 0;
}

int green_yield() {
    green_t *susp = running;
    
    insert(susp);               //set susp last in queue
    green_t *next = retrieve(); //get first in queue
    
    running = next;
    swapcontext(susp->context, next->context);
    
    return 0;
}

int green_join(green_t *thread, void **res) {
    if (!thread->zombie) {
        green_t *susp = running;
        susp->join = thread;
        green_t *next = retrieve();
        
        running = next;
        swapcontext(susp->context, next->context);
    }
    *res = thread->retval;
    free(thread->context);
    return 0;
}