/*
* Tencent is pleased to support the open source community by making Libco available.

* Copyright (C) 2014 THL A29 Limited, a Tencent company. All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License"); 
* you may not use this file except in compliance with the License. 
* You may obtain a copy of the License at
*
*	http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, 
* software distributed under the License is distributed on an "AS IS" BASIS, 
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
* See the License for the specific language governing permissions and 
* limitations under the License.
*/

#include "co_routine.h"
#include <queue>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int co_count = 0;

int64_t get_clock_now()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    return (int64_t)(ts.tv_sec * 1000L + ts.tv_nsec / 1000000L);
}

void *coroutine(void *args)
{
    while (true) {
        co_count++;
        co_yield_ct();
    }
    return NULL;
}

// test countine switch speed
int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("usage: %s count\n", argv[0]);
        return 1;
    }
    int cnt = atoi(argv[1]);

    stCoRoutine_t *routine;
    co_create(&routine, NULL, coroutine, NULL);

    int64_t begin = get_clock_now();
    for(int i = 0; i < cnt; i++) {
        co_resume(routine);
    }

    printf("co_count=%d time=%ldms\n", co_count, get_clock_now() - begin);
    return 0;
}
