
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>

struct timespec t_start, t_stop;


void *g_func(void *arg) {
    int loops = *(int*)arg;
    while (loops > 0) {
        loops--;
        sched_yield();
    }
}

void *pt_func(void *arg) {
    int loops = *(int*)arg;
    while (loops > 0) {
        loops--;
        sched_yield();
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("usage ./bench <maxthreads> <threadincr> <loops/thr>");
        exit(0);
    }

    int maxthreads = atoi(argv[1]);
    int incr = atoi(argv[2]);
    int loops = atoi(argv[3]);

    for (int i = 5; i < maxthreads; i = i + incr) {
        pthread_t thr[i];

        /*******************************************************************
         ********************** PTHREAD LIBRARY ******************************
         ******************************************************************/
        clock_gettime(CLOCK_MONOTONIC_COARSE, &t_start);
        
        int j;
        for (j = 0; j < i; j++) {
            pthread_create(&thr[j], NULL, pt_func, &loops); //create i threads
        }

        for (j = 0; j < i; j++) {
            pthread_join(thr[j], NULL);
        }

        clock_gettime(CLOCK_MONOTONIC_COARSE, &t_stop);

        long pt_wall_sec  = t_stop.tv_sec - t_start.tv_sec;
        long pt_wall_nsec = t_stop.tv_nsec - t_start.tv_nsec;
        long pt_wall_msec = (pt_wall_sec * 1000) + (pt_wall_nsec / 1000000);
        printf("done %d threads: %ld ms\n", i, pt_wall_msec);


        /*******************************************************************
         ********************** GREEN LIBRARY ******************************
         ******************************************************************/
        clock_gettime(CLOCK_MONOTONIC_COARSE, &t_start);
        
        int j;
        for (j = 0; j < i; j++) {
            pthread_create(&thr[j], NULL, g_func, &loops); //create i threads
        }

        for (j = 0; j < i; j++) {
            pthread_join(thr[j], NULL);
        }

        clock_gettime(CLOCK_MONOTONIC_COARSE, &t_stop);

        long g_wall_sec  = t_stop.tv_sec - t_start.tv_sec;
        long g_wall_nsec = t_stop.tv_nsec - t_start.tv_nsec;
        long g_wall_msec = (g_wall_sec * 1000) + (g_wall_nsec / 1000000);
        printf("done %d threads: %ld ms\n\n", i, g_wall_msec);
    }


}