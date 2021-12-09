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

#ifndef __CO_ROUTINE_INNER_H__
#define __CO_ROUTINE_INNER_H__

#include "co_routine.h"
#include "coctx.h"

struct stCoRoutineEnv_t;

struct stCoSpec_t {
    void *value;
};

struct stStackMem_t {
    int stack_size;
    char *stack_bp; //stack_buffer + stack_size
    char *stack_buffer;
};

struct stCoRoutineLink_t {
    stCoRoutine_t *head;
    stCoRoutine_t *tail;
};

struct stCoRoutine_t {
    stCoRoutineEnv_t *env;
    pfn_co_routine_t pfn;
    void *arg;
    coctx_t ctx;

    char cStart;
    char cEnd;
    char cIsMain;

    stStackMem_t *stack_mem;
    stCoSpec_t aSpec[1024];

    stCoRoutine_t *pPrev;
    stCoRoutine_t *pNext;
    stCoRoutineLink_t *pLink;
};

#endif
