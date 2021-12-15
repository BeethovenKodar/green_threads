
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "green.h"

int threads = 0;
int flag = 0;
pthread_cond_t condpthr;
green_cond_t cond;
green_mutex_t mutex;

void *demanding(long int loop) {
    int i = 0;
    int toggle = 1;
    while (i < loop) {
        toggle = (toggle + 1) % 2;
        i++;
    }
}

volatile int testme = 0;
void *test_locks(void *arg) {
    int id = *(int*)arg;
    printf("LOCKS %d START\n", id);
    int loop = 50000;
    while (loop > 0) {
        green_mutex_lock(&mutex);
        demanding(10000);
        testme++;
        green_mutex_unlock(&mutex);
        loop--;
    }
    printf("LOCKS %d DONE\n", id);
}

void *test_main(void *arg) {
    int id = *(int*)arg;
    int loop = 500000;

    printf("MAIN TEST %d\n", id);
    while (loop > 0) {
        green_mutex_lock(&mutex);
        while (flag != id) {
            green_cond_waitl(&cond, &mutex);
        }
        flag = (id + 1) % 2;
        green_mutex_unlock(&mutex);
        green_cond_signal(&cond);
        loop--;
    }
    printf("MAIN TEST %d DONE\n", id);
}

void *test_isr(void *arg) {
    int id = *(int*)arg;
    int loop = 50000;

    printf("ISR %d\n", id);
    while(loop > 0) {
        if (id == 0) {
            demanding(100000);
        } else {
            demanding(10000);
        }
        //printf("thread %d: %d\n", id, loop);
        loop--;
    }
    printf("ISR %d DONE\n", id);
}

/* test conditional variables */
void *test_cond(void *arg) {
    int id = *(int*)arg;
    printf("CONDITIONAL %d\n", id);
    int loop = 5000;
    while (loop > 0) {
        if (flag == id) {
            loop--;
            if (id + 1 == 2) {
                flag = 0;
            } else {                //här sätts flaggan fel, alla trådar somnar
                flag = id + 1;
            }
            green_cond_signal(&cond);
        } else {
            green_cond_wait(&cond);
        }
    }
    printf("CONDITIONAL %d DONE\n", id);
}

/* yielding for each loop */
void *test_yield(void *arg) {
    int id = *(int*)arg;
    printf("YIELD %d\n", id);
    int loop = 100000;
    while(loop > 0) {
        printf("thread %d: %d\n", id, loop);
        loop--;
        green_yield();
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("usage ./test <type>\n");
        exit(EXIT_FAILURE);
    }

    int mode = atoi(argv[1]);

    void *fun[6] = {
        test_isr, test_cond, test_yield, 
        test_locks, test_waitlock, test_main
    };
    
    int a0 = 0;
    int a1 = 1;
    int a2 = 2;
    green_t g0, g1, g2;
    
    if (mode == 1 || mode == 4 || mode == 5) {
        green_cond_init(&cond);
    }
    if (mode == 3 || mode == 4 || mode == 5) {
        green_mutex_init(&mutex);
    }

    green_create(&g0, fun[mode], &a0);
    green_create(&g1, fun[mode], &a1);
    green_create(&g2, fun[mode], &a2);
    
    green_join(&g0, NULL);
    green_join(&g1, NULL);
    green_join(&g2, NULL);
    if (mode == 3) {
        printf("testme: %d\n", testme);
    }
    printf("done\n");
    return 0;
}