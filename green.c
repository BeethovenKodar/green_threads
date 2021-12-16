
#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
#include <assert.h>
#include <signal.h>
#include <sys/time.h>
#include "green.h"


#define FALSE 0
#define TRUE 1
#define PERIOD 100
#define STACK_SIZE 4096


static ucontext_t main_cntx = {0};
static green_t main_green = {&main_cntx, NULL, NULL, NULL, NULL, NULL, TRUE};
green_t *running = &main_green;

static queue_t *ready_q = &(queue_t) {
    .first = NULL,
    .last = NULL
};

static sigset_t block;
void timer_handler(int sig);


static void init() __attribute__((constructor));

void init() {
    getcontext(&main_cntx);

    sigemptyset(&block);
    sigaddset(&block, SIGVTALRM);

    struct sigaction act = {0};
    struct timeval interval;
    struct itimerval period;

    act.sa_handler = timer_handler;
    assert(sigaction(SIGVTALRM, &act, NULL) == 0);
    interval.tv_sec = 0;
    interval.tv_usec = PERIOD;
    period.it_interval = interval;
    period.it_value = interval;
    setitimer(ITIMER_VIRTUAL, &period, NULL);
}

void printlength(green_t *first) {
    sigprocmask(SIG_BLOCK, &block, NULL);
    green_t *current = first;
    int size = 0;
    printf("queue: ");
    while (current != NULL) {
        size++;
        printf("%d, ", *(int*)current->arg);
        current = current->next;
    }
    printf("\t\tlength___: %d\n", size);
    sigprocmask(SIG_UNBLOCK, &block, NULL);
}

void insert(green_t *new, queue_t *queue) {
    new->next = NULL;
    if (queue->first == NULL) {
        queue->first = new;
        queue->last = new;
    } else {
        queue->last->next = new;
        queue->last = new;
    }
    return;
}

green_t *detach(queue_t *queue) {
    green_t *use = queue->first;
    if (use == NULL) {
        fprintf(stderr, "\nqueue null\n");
        printlength(ready_q->first);
        printlength(queue->first);
        exit(EXIT_FAILURE);
    }
    queue->first = use->next;
    return use;
}

void green_cond_init(green_cond_t *cond) {
    sigprocmask(SIG_BLOCK, &block, NULL);
    queue_t *queue = (queue_t*)malloc(sizeof(queue_t));
    queue -> first = NULL;
    queue -> last = NULL;
    cond -> queue = queue;
    sigprocmask(SIG_UNBLOCK, &block, NULL);
}

void green_cond_wait(green_cond_t *cond) {
    sigprocmask(SIG_BLOCK, &block, NULL);
    green_t *suspended = running;
    //printlength(cond->queue->first);
    green_t *next = detach(ready_q);
    running = next;
    insert(suspended, cond->queue);
    swapcontext(suspended->context, next->context);
    sigprocmask(SIG_UNBLOCK, &block, NULL);
}

/* RÖR FAN INTE SIGPROC */
void green_cond_signal(green_cond_t *cond) {
    sigprocmask(SIG_BLOCK, &block, NULL);
    if (cond->queue->first == NULL) {
        sigprocmask(SIG_UNBLOCK, &block, NULL);
        return;
    }
    green_t *wake = detach(cond->queue);
    insert(wake, ready_q);
    sigprocmask(SIG_UNBLOCK, &block, NULL);
}

int green_mutex_init(green_mutex_t *mutex) {
    mutex->taken = FALSE;
    queue_t *queue = (queue_t*)malloc(sizeof(queue_t));
    queue->first = NULL;
    queue->last = NULL;
    mutex->queue = queue;
    return 0;
}

/* RÖR FAN INTE SIGPROC */
int green_mutex_lock(green_mutex_t *mutex) {
    sigprocmask(SIG_BLOCK, &block, NULL);
    green_t *susp = running;
    if (mutex->taken) {
        insert(susp, mutex->queue);
        green_t *next = detach(ready_q);
        running = next;
        swapcontext(susp->context, next->context);
    } else {
        mutex->taken = TRUE;
    }
    sigprocmask(SIG_UNBLOCK, &block, NULL);
    return 0;
}

/* RÖR FAN INTE SIGPROC */
int green_mutex_unlock(green_mutex_t *mutex) {
    sigprocmask(SIG_BLOCK, &block, NULL);
    if (mutex->queue->first != NULL) {
        green_t *wake = detach(mutex->queue);
        insert(wake, ready_q);
    } else {
        mutex->taken = FALSE;
    }
    sigprocmask(SIG_UNBLOCK, &block, NULL);
    return 0;
}

void green_cond_waitl(green_cond_t *cond, green_mutex_t *mutex) {
    sigprocmask(SIG_BLOCK, &block, NULL);
    green_t *susp = running;
    insert(susp, cond->queue);
   
    if (mutex != NULL) {
        mutex->taken = FALSE;
        if (mutex->queue->first != NULL) {
            green_t *wake = detach(mutex->queue);
            insert(wake, ready_q);
        }
    }
    green_t *next = detach(ready_q);
    running = next;
    swapcontext(susp->context, next->context);
    
    if (mutex != NULL) {
        if (mutex->taken) {
            green_mutex_lock(mutex);
        } else {
            mutex->taken = TRUE;
        }
    }
    sigprocmask(SIG_UNBLOCK, &block, NULL);
}

/* RÖR FAN INTE SIGPROC */
void green_thread() {
    green_t *this = running;
  
    void *result = (*this->fun)(this->arg);

    sigprocmask(SIG_BLOCK, &block, NULL);
    if (this->join != NULL) {
        insert(this->join, ready_q);    //place waiting (joining) thread in ready queue
    }
    
    this->retval = result;              //save result of execution
    this->zombie = TRUE;                //zombie state

    green_t *next = detach(ready_q);    //find the next thread to run
    running = next;
    sigprocmask(SIG_UNBLOCK, &block, NULL);
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

    sigprocmask(SIG_BLOCK, &block, NULL);
    insert(new, ready_q);
    sigprocmask(SIG_UNBLOCK, &block, NULL);

    return 0;
}

int green_yield() {
    sigprocmask(SIG_BLOCK, &block, NULL);
    green_t *susp = running;
    insert(susp, ready_q);              //set susp last in queue

    green_t *next = detach(ready_q);    //get first in queue
    running = next;
    
    sigprocmask(SIG_UNBLOCK, &block, NULL);
    swapcontext(susp->context, next->context);

    return 0;
}

int green_join(green_t *thread, void **res) {
    sigprocmask(SIG_BLOCK, &block, NULL);
    if (!thread->zombie) {
        green_t *susp = running;
        thread->join = susp;
        green_t *next = detach(ready_q);
        running = next;
        sigprocmask(SIG_UNBLOCK, &block, NULL);
        swapcontext(susp->context, next->context);
        sigprocmask(SIG_BLOCK, &block, NULL);
    }
    if (res != NULL) {
        *res = thread->retval;
    }
    free(thread->context);
    sigprocmask(SIG_UNBLOCK, &block, NULL);
    
    return 0;
}

void timer_handler(int sig) {
    if(sig){};
    sigprocmask(SIG_BLOCK, &block, NULL);
    green_t *susp = running;
    insert(susp, ready_q);
    green_t *next = detach(ready_q);
    running = next;
    sigprocmask(SIG_UNBLOCK, &block, NULL);
    swapcontext(susp->context, next->context);
}