/*
* Tencent is pleased to support the open source community by making Libco
available.

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

#include "coctx.h"
#include <stdio.h>
#include <string.h>

static __thread void *pfn_userdata = NULL;

int coctx_init(coctx_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    return 0;
}

int coctx_make(coctx_t *ctx, void *userdata)
{
    char *sp = ctx->ss_sp + ctx->ss_size - sizeof(void *);
    sp = (char *)((unsigned long)sp & -16LL);

    if (MD_SETJMP(ctx->context))
        CoRoutineFunc(pfn_userdata);
    ctx->userdata = userdata;
    MD_GET_SP(ctx) = (long)sp;
    return 0;
}

void coctx_swap(coctx_t *cur, coctx_t *pending)
{
    if (MD_SETJMP(cur->context))
        return;
    pfn_userdata = pending->userdata;
    MD_LONGJMP(pending->context, 1);
}