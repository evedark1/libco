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

void AddSuccCnt()
{
    static int iSuccCnt = 0;
    static int iTime = 0;

	int now = time(NULL);
	if (now >iTime)
	{
		printf("time %d Succ Cnt %d\n", iTime, iSuccCnt);
		iTime = now;
		iSuccCnt = 0;
	}
	else
	{
		iSuccCnt++;
	}
}

void *Producer(void *args)
{
    stCoCond_t *cond = (stCoCond_t*)args;
    while (true) {
        co_cond_signal(cond);
        co_poll(co_get_epoll_ct(), NULL, 0, 1);
    }
    return NULL;
}

void *Consumer(void *args)
{
    stCoCond_t *cond = (stCoCond_t*)args;
    while (true) {
        co_cond_timedwait(cond, -1);
        AddSuccCnt();
    }
    return NULL;
}

int main(int argc, char **argv)
{
    int cnt = atoi(argv[1]);

    for(int i = 0; i < cnt; i++) {
        stCoCond_t *cond = co_cond_alloc();
        stCoRoutine_t *consumer_routine;
        co_create(&consumer_routine, NULL, Consumer, cond);
        co_resume(consumer_routine);

        stCoRoutine_t *producer_routine;
        co_create(&producer_routine, NULL, Producer, cond);
        co_resume(producer_routine);
    }

    co_eventloop(co_get_epoll_ct(), NULL, NULL);
    return 0;
}
