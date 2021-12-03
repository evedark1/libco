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

#ifndef __CO_ROUTINE_H__
#define __CO_ROUTINE_H__

#include <pthread.h>
#include <stdint.h>
#include <sys/poll.h>

//1.struct

struct stCoRoutine_t;

struct stCoRoutineAttr_t {
    int stack_size;

    stCoRoutineAttr_t()
    {
        stack_size = 128 * 1024;
    }
};

struct stCoEpoll_t;
typedef int (*pfn_co_eventloop_t)(void *);
typedef void *(*pfn_co_routine_t)(void *);
typedef void (*pfn_co_call_t)(void *);

//2.co_routine

int co_create(stCoRoutine_t **co, const stCoRoutineAttr_t *attr, pfn_co_routine_t routine, void *arg);
void co_resume(stCoRoutine_t *co);
void co_yield_ct(); //ct = current thread
stCoRoutine_t *co_self();

//3.specific

int co_setspecific(pthread_key_t key, const void *value);
void *co_getspecific(pthread_key_t key);

//4.event

void co_async_call(stCoEpoll_t *ctx, pfn_co_call_t func, void *arg);
int co_poll(stCoEpoll_t *ctx, struct pollfd fds[], nfds_t nfds, int timeout_ms);
void co_eventloop(stCoEpoll_t *ctx, pfn_co_eventloop_t pfn, void *arg);

stCoEpoll_t *co_get_epoll_ct(); //ct = current thread

//5.sync
struct stCoCond_t;

stCoCond_t *co_cond_alloc();
int co_cond_free(stCoCond_t *cc);

int co_cond_signal(stCoCond_t *);
int co_cond_broadcast(stCoCond_t *);
int co_cond_timedwait(stCoCond_t *, int timeout_ms);

#endif
