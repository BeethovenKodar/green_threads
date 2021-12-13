
#include <stdio.h>
#include <stdlib.h>
#include "green.h"

int flag = 0;
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

int testme = 0;
void *test_locks(void *arg) {
    printf("LOCKS\n");
    int id = *(int*)arg;
    int loop = 5000;
    while (loop > 0) {
        green_mutex_lock(&mutex);
        demanding(100);
        testme++;
        printf("testme: %d\n", testme);
        green_mutex_unlock(&mutex);
        loop--;
    }
}

void *test_isr(void *arg) {
    printf("ISR\n");
    int id = *(int*)arg;
    int loop = 5;
    while(loop > 0) {
        if (id == 0) {
            demanding(10000000);
        } else {
            demanding(1000000);
        }
        printf("thread %d: %d\n", id, loop);
        loop--;
    }
}

/* test conditional variables */
void *test_cond(void *arg) {
    printf("CONDITIONAL\n");
    int id = *(int*)arg;
    int loop = 5;
    while (loop > 0) {
        if (flag == id) {
            printf("thread %d: %d\n", id, loop);
            loop--;
            if (id + 1 == 3) {
                flag = 0;
            } else {
                flag = id + 1;
            }
            green_cond_signal(&cond);
        } else {
            green_cond_wait(&cond);
        }
    }
}

/* yielding for each loop */
void *test_yield(void *arg) {
    printf("YIELD\n");
    int i = *(int*)arg;
    int loop = 5;
    while(loop > 0) {
        printf("thread %d: %d\n", i, loop);
        loop--;
        green_yield();
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("usage ./test <type>");
        exit(EXIT_FAILURE);
    }
    int mode = atoi(argv[1]);
    void *fun[4] = {test_isr, test_cond, test_yield, test_locks};
    int a0 = 0;
    int a1 = 1;
    int a2 = 2;
    green_t g0, g1, g2;
    
    green_cond_init(&cond);
    green_mutex_init(&mutex);

    green_create(&g0, fun[mode], &a0);
    green_create(&g1, fun[mode], &a1);
    green_create(&g2, fun[mode], &a2);
    
    green_join(&g0, NULL);
    green_join(&g1, NULL);
    green_join(&g2, NULL);
    
    printf("done\n");
    return 0;
}