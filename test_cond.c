
#include "green.h"
#include <stdio.h>

int flag = 0;
//green_cond_t cond;

void *test(void *arg) {
    green_cond_init(&cond);
    int id = *(int*)arg;
    int loop = 4;
    while(loop > 0) {
        if (flag == id) {
            printf("thread %d: %d\n", id, loop);
            loop--;
            flag = (id + 1) % 2;
            green_cond_signal(&cond);
        } else {
            green_cond_wait(&cond);
        }
    }
}