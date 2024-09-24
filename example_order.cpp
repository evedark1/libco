#include "co_routine.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void *coroutine2(void *arg)
{
    printf("coroutine2 1\n");
    co_poll(co_get_epoll_ct(), NULL, 0, 500);
    printf("coroutine2 2\n");
    return NULL;
}

void *coroutine1(void *arg)
{
    printf("coroutine1 1\n");
    stCoRoutine_t *co;
    co_create(&co, NULL, coroutine2, NULL);
    co_resume(co);

    printf("coroutine1 2\n");
    co_poll(co_get_epoll_ct(), NULL, 0, 1000);
    printf("coroutine1 3\n");
    return NULL;
}

/**
 * desired output:
 * main 1
 * coroutine1 1
 * coroutine2 1
 * coroutine1 2
 * main 2
 * coroutine2 2
 * coroutine1 3
*/
int main(int argc, char **argv)
{
    printf("main 1\n");
    stCoRoutine_t *co;
    co_create(&co, NULL, coroutine1, NULL);
    co_resume(co);

    printf("main 2\n");
    co_eventloop(co_get_epoll_ct(), NULL, NULL);
    return 0;
}
