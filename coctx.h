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

#ifndef __CO_CTX_H__
#define __CO_CTX_H__
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "md.h"

typedef struct coctx_t_ {
    _st_jmp_buf_t context;
    void *userdata;

    size_t ss_size;
    char *ss_sp;
} coctx_t;

int coctx_init(coctx_t *ctx);
int coctx_make(coctx_t *ctx, void *userdata);
void coctx_swap(coctx_t *cur, coctx_t *pending);

extern void CoRoutineFunc(void *arg);

#ifdef __cplusplus
}
#endif

#endif
