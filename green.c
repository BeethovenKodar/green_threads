
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

//"last" element in queue
static green_t dummy = {NULL, NULL, NULL, NULL, NULL, NULL, TRUE};
green_t *first = &dummy;
green_t *last = &dummy;

static void init() __attribute__((constructor));

void init() {
    getcontext(&main_cntx);
}

void insert(green_t *new) {
    new->next = NULL;
    last->next = new;
    last = new;
}

green_t *retrieve() {
    green_t *use = first->next;
    if (use == NULL) {
        fprintf(stderr, "ready list null\n");
        exit(EXIT_FAILURE);
    }

    if (use == last) {      //one thread in queue + sentinel
        last = first;
        first->next = NULL;
    } else {                //2+ threads in queue + sentinel
        first->next = use->next;
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
        thread->join = susp;
        
        green_t *next = retrieve();
        running = next;
        swapcontext(susp->context, next->context);
    }
    if (res != NULL) {
        *res = thread->retval;
    }
    free(thread->context);
    return 0;
}