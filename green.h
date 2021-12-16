
#include <ucontext.h>

typedef struct green_t {
    ucontext_t *context;
    void *(*fun)(void*);
    void *arg;
    struct green_t *next;
    struct green_t *join;
    void *retval;
    int zombie;
} green_t;

typedef struct queue_t {
    green_t *first;
    green_t *last;
} queue_t;

typedef struct green_cond_t {
    queue_t *queue;
} green_cond_t;

typedef struct green_mutex_t {
    volatile int taken;
    queue_t *queue;
} green_mutex_t;

//threads
int green_create(green_t *thread, void *(*fun)(void*), void *arg);
int green_join(green_t *thread, void **val);
int green_yield();

//conditional variables
void green_cond_init(green_cond_t*);
void green_cond_wait(green_cond_t*);
void green_cond_waitl(green_cond_t*, green_mutex_t*);
void green_cond_signal(green_cond_t*);

//mutexes
int green_mutex_init(green_mutex_t*);
int green_mutex_lock(green_mutex_t*);
int green_mutex_unlock(green_mutex_t*);
