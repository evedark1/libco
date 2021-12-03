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

void *coroutine(void *arg)
{
    co_poll(co_get_epoll_ct(), NULL, 0, 1000);
    return NULL;
}

void *create_coroutine(void *arg)
{
    int cnt = *(int*)arg;
    while(true) {
        for(int i = 0; i < cnt; i++) {
            stCoRoutine_t *co;
            co_create(&co, NULL, coroutine, NULL);
            co_resume(co);
        }
        co_poll(co_get_epoll_ct(), NULL, 0, 2000);
    }
    return NULL;
}

int main(int argc, char **argv)
{
    int cnt = atoi(argv[1]);

    stCoRoutine_t *co;
    co_create(&co, NULL, create_coroutine, &cnt);
    co_resume(co);

    co_eventloop(co_get_epoll_ct(), NULL, NULL);
    return 0;
}
