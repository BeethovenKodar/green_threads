
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include "green.h"

struct timespec t_start, t_stop;
pthread_mutex_t pt_mutex;
green_mutex_t g_mutex;
pthread_cond_t pt_cond;
green_cond_t g_cond;

volatile int loops;
volatile int threads;
volatile int flag;
volatile int count;

void *g_locks(void *arg) {
    int loop = loops;
    while (loop-- > 0) {
        green_mutex_lock(&g_mutex);
        count++;
        green_mutex_unlock(&g_mutex);
    }
}

void *pt_locks(void *arg) {
    int loop = loops;
    while (loop-- > 0) {
        pthread_mutex_lock(&pt_mutex);
        count++;
        pthread_mutex_unlock(&pt_mutex);
    }
}

void *g_func(void *arg) {
    int id = *(int*)arg;
    int loop = loops;
    while (loop > 0) {
        green_mutex_lock(&g_mutex);
        while (flag != id) {
            green_cond_signal(&g_cond);
            green_cond_waitl(&g_cond, &g_mutex);
        }
        count++;
        flag = (id + 1) % threads;
        green_cond_signal(&g_cond);
        green_mutex_unlock(&g_mutex);
        loop--;
    }
}

void *pt_func(void *arg) {
    int id = *(int*)arg;
    int loop = loops;
    while (loop > 0) {
        pthread_mutex_lock(&pt_mutex);
        while (flag != id) {
            pthread_cond_signal(&pt_cond);
            pthread_cond_wait(&pt_cond, &pt_mutex);
        }
        flag = (id + 1) % threads;
        pthread_mutex_unlock(&pt_mutex);
        pthread_cond_signal(&pt_cond);
        loop--;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("usage ./bench <maxthreads> <loops per thr>\n");
        exit(0);
    }

    int maxthreads = atoi(argv[1]);
    loops = atoi(argv[2]);

    for (int i = maxthreads; i <= maxthreads; i++) {
        int j;
        threads = i;
        
        /******************************************************************
         ********************** PTHREAD LIBRARY ***************************
         ******************************************************************/
        flag = 0;
        pthread_t pt_thr[threads];

        clock_gettime(CLOCK_MONOTONIC_COARSE, &t_start);

        pthread_mutex_init(&pt_mutex, NULL);
        pthread_cond_init(&pt_cond, NULL);
        for (j = 0; j < i; j++) {
            int *pt_j = (int*)malloc(sizeof(int));
            *pt_j = j;
            pthread_create(&pt_thr[j], NULL, pt_func, pt_j);
        }
        for (j = 0; j < i; j++) {
            pthread_join(pt_thr[j], NULL);
        }

        clock_gettime(CLOCK_MONOTONIC_COARSE, &t_stop);

        pthread_mutex_destroy(&pt_mutex);
        pthread_cond_destroy(&pt_cond);

        long pt_wall_sec  = t_stop.tv_sec - t_start.tv_sec;
        long pt_wall_nsec = t_stop.tv_nsec - t_start.tv_nsec;
        long pt_wall_msec = (pt_wall_sec * 1000) + (pt_wall_nsec / 1000000);
        printf("%3d  %4ld\n", i, pt_wall_msec);


        /******************************************************************
         ************************ GREEN LIBRARY ***************************
         ******************************************************************/
        flag = 0;
        count = 0;
        green_t g_thr[i];

        clock_gettime(CLOCK_MONOTONIC_COARSE, &t_start);
        green_mutex_init(&g_mutex);
        green_cond_init(&g_cond);
        for (j = 0; j < i; j++) {
            int *g_j = (int*)malloc(sizeof(int));
            *g_j = j;
            green_create(&g_thr[j], g_func, g_j);
        }
        for (j = 0; j < i; j++) {
            green_join(&g_thr[j], NULL);
        }
        clock_gettime(CLOCK_MONOTONIC_COARSE, &t_stop);

        long g_wall_sec  = t_stop.tv_sec - t_start.tv_sec;
        long g_wall_nsec = t_stop.tv_nsec - t_start.tv_nsec;
        long g_wall_msec = (g_wall_sec * 1000) + (g_wall_nsec / 1000000);
        printf("%3d  %4ld\n\n", i, g_wall_msec);

        if (count != (threads * loops)) {
            fprintf(stderr, "count didn't work, got %d instead of %d\n", count, threads * loops);
            exit(EXIT_FAILURE);
        }
        count = 0;
    }
    return 0;
}