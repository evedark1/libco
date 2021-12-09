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
#include "co_epoll.h"
#include "co_routine_inner.h"

#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <poll.h>
#include <sys/time.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/syscall.h>

static uint64_t GetTickNow()
{
    struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)(ts.tv_sec * 1000L + ts.tv_nsec / 1000000L);
}

/* 侵入式链表实现
struct TNode {
    TNode *pPrev;
    TNode *pNext;
    TLink *pLink;
};

struct TLink {
	TNode *head;
    TNode *tail;
};
*/

// l.erase(ap)
template <class TNode, class TLink>
void RemoveFromLink(TNode *ap)
{
    TLink *lst = ap->pLink;
    if (!lst)
        return;
    assert(lst->head && lst->tail);

    if (ap == lst->head) {
        lst->head = ap->pNext;
        if (lst->head) {
            lst->head->pPrev = NULL;
        }
    } else {
        if (ap->pPrev) {
            ap->pPrev->pNext = ap->pNext;
        }
    }

    if (ap == lst->tail) {
        lst->tail = ap->pPrev;
        if (lst->tail) {
            lst->tail->pNext = NULL;
        }
    } else {
        ap->pNext->pPrev = ap->pPrev;
    }

    ap->pPrev = ap->pNext = NULL;
    ap->pLink = NULL;
}

// apLink.push_back(ap)
template <class TNode, class TLink>
void inline AddTail(TLink *apLink, TNode *ap)
{
    if (ap->pLink) {
        return;
    }
    if (apLink->tail) {
        apLink->tail->pNext = (TNode *)ap;
        ap->pNext = NULL;
        ap->pPrev = apLink->tail;
        apLink->tail = ap;
    } else {
        apLink->head = apLink->tail = ap;
        ap->pNext = ap->pPrev = NULL;
    }
    ap->pLink = apLink;
}

// apLink.pop_front()
template <class TNode, class TLink>
void inline PopHead(TLink *apLink)
{
    if (!apLink->head) {
        return;
    }
    TNode *lp = apLink->head;
    if (apLink->head == apLink->tail) {
        apLink->head = apLink->tail = NULL;
    } else {
        apLink->head = apLink->head->pNext;
    }

    lp->pPrev = lp->pNext = NULL;
    lp->pLink = NULL;

    if (apLink->head) {
        apLink->head->pPrev = NULL;
    }
}

// apLink.merge(apOther)
template <class TNode, class TLink>
void inline Join(TLink *apLink, TLink *apOther)
{
    if (!apOther->head) {
        return;
    }
    TNode *lp = apOther->head;
    while (lp) {
        lp->pLink = apLink;
        lp = lp->pNext;
    }
    lp = apOther->head;
    if (apLink->tail) {
        apLink->tail->pNext = (TNode *)lp;
        lp->pPrev = apLink->tail;
        apLink->tail = apOther->tail;
    } else {
        apLink->head = apOther->head;
        apLink->tail = apOther->tail;
    }

    apOther->head = apOther->tail = NULL;
}

/////////////////for copy stack //////////////////////////
static stStackMem_t *co_alloc_stackmem(unsigned int stack_size)
{
    stStackMem_t *stack_mem = (stStackMem_t *)malloc(sizeof(stStackMem_t));
    stack_mem->stack_size = stack_size;
    stack_mem->stack_buffer = (char *)malloc(stack_size);
    stack_mem->stack_bp = stack_mem->stack_buffer + stack_size;
    return stack_mem;
}

// ----------------------------------------------------------------------------
struct stTimeoutItemLink_t;

struct stTimeoutItem_t {
    stTimeoutItem_t *pPrev;
    stTimeoutItem_t *pNext;
    stTimeoutItemLink_t *pLink;

    pfn_co_call_t func;
    void *arg;

    uint64_t expireTime;
    bool bAutoFree;
};

struct stTimeoutItemLink_t {
    stTimeoutItem_t *head;
    stTimeoutItem_t *tail;
};

// 时间轮实现的定时器
struct stTimeout_t {
private:
    static const size_t ItemSize = 60 * 1000;

    uint64_t start;
    size_t startIdx;
    stTimeoutItemLink_t items[ItemSize];

public:
    stTimeout_t()
    {
        start = GetTickNow();
        startIdx = 0;
    }

    void addTimeout(stTimeoutItem_t *apItem, uint64_t now)
    {
        assert(now >= start);
        assert(apItem->expireTime >= now);

        uint64_t diff = apItem->expireTime - start;
        if (diff >= ItemSize) {
            diff = ItemSize - 1;
        }
        size_t idx = (startIdx + diff) % ItemSize;
        AddTail(items + idx, apItem);
    }

    void takeAllTimeout(uint64_t now, stTimeoutItemLink_t *apResult)
    {
        assert(now >= start);

        uint64_t cnt = now - start + 1;
        if (cnt > ItemSize) {
            cnt = ItemSize;
        }
        for (uint64_t i = 0; i < cnt; i++) {
            size_t idx = (startIdx + i) % ItemSize;
            Join<stTimeoutItem_t, stTimeoutItemLink_t>(apResult, items + idx);
        }
        start = now;
        startIdx += cnt - 1;
    }
};

// ------------------------------- stCoRoutineEnv_t --------------------------------
struct stCoRoutineEnv_t {
    stCoRoutine_t *pCallStack[128];
    int iCallStackSize;
    stCoEpoll_t *pEpoll;
    stCoRoutineLink_t activeRoutine;
};

static __thread stCoRoutineEnv_t *gCoEnvPerThread = NULL;

struct stCoRoutine_t *co_create_env(stCoRoutineEnv_t *env, const stCoRoutineAttr_t *attr, pfn_co_routine_t pfn, void *arg);
stCoEpoll_t *AllocEpoll();

static void co_init_curr_thread_env()
{
    gCoEnvPerThread = (stCoRoutineEnv_t *)calloc(1, sizeof(stCoRoutineEnv_t));
    stCoRoutineEnv_t *env = gCoEnvPerThread;

    env->iCallStackSize = 0;
    struct stCoRoutine_t *self = co_create_env(env, NULL, NULL, NULL);
    self->cIsMain = 1;

    coctx_init(&self->ctx);

    env->pCallStack[env->iCallStackSize++] = self;

    env->pEpoll = AllocEpoll();
}

static stCoRoutineEnv_t *co_get_curr_thread_env()
{
    return gCoEnvPerThread;
}

static stCoRoutine_t *GetCurrCo(stCoRoutineEnv_t *env)
{
    return env->pCallStack[env->iCallStackSize - 1];
}

// ------------------------------ co_routine ------------------------------
struct stCoRoutine_t *co_create_env(stCoRoutineEnv_t *env, const stCoRoutineAttr_t *attr, pfn_co_routine_t pfn, void *arg)
{
    stCoRoutineAttr_t at;
    if (attr) {
        memcpy(&at, attr, sizeof(at));
    }
    if (at.stack_size <= 0) {
        at.stack_size = 128 * 1024;
    } else if (at.stack_size > 1024 * 1024 * 8) {
        at.stack_size = 1024 * 1024 * 8;
    }

    if (at.stack_size & 0xFFF) {
        at.stack_size &= ~0xFFF;
        at.stack_size += 0x1000;
    }

    stCoRoutine_t *lp = (stCoRoutine_t *)malloc(sizeof(stCoRoutine_t));
    memset(lp, 0, sizeof(stCoRoutine_t));

    lp->env = env;
    lp->pfn = pfn;
    lp->arg = arg;

    stStackMem_t *stack_mem = co_alloc_stackmem(at.stack_size);
    lp->stack_mem = stack_mem;
    lp->ctx.ss_sp = stack_mem->stack_buffer;
    lp->ctx.ss_size = at.stack_size;

    return lp;
}

int co_create(stCoRoutine_t **ppco, const stCoRoutineAttr_t *attr, pfn_co_routine_t pfn, void *arg)
{
    if (!co_get_curr_thread_env()) {
        co_init_curr_thread_env();
    }
    stCoRoutine_t *co = co_create_env(co_get_curr_thread_env(), attr, pfn, arg);
    *ppco = co;
    return 0;
}

static void co_release(stCoRoutine_t *co)
{
    free(co->stack_mem->stack_buffer);
    free(co->stack_mem);
    free(co);
}

static void co_swap(stCoRoutine_t *curr, stCoRoutine_t *pending_co)
{
    //swap context
    coctx_swap(&(curr->ctx), &(pending_co->ctx));
}

static void co_yield_env(stCoRoutineEnv_t *env)
{
    stCoRoutine_t *last = env->pCallStack[env->iCallStackSize - 2];
    stCoRoutine_t *curr = env->pCallStack[env->iCallStackSize - 1];
    env->iCallStackSize--;
    co_swap(curr, last);
}

static int CoRoutineFunc(stCoRoutine_t *co, void *)
{
    stCoRoutineEnv_t *env = co->env;
    AddTail(&(env->activeRoutine), co);

    if (co->pfn) {
        co->pfn(co->arg);
    }
    co->cEnd = 1;

    RemoveFromLink<stCoRoutine_t, stCoRoutineLink_t>(co);
    co_async_call(env->pEpoll, (pfn_co_call_t)co_release, co);
    co_yield_env(co->env);
	// never go here, because routine has exist and yield
    return 0;
}

void co_resume(stCoRoutine_t *co)
{
    stCoRoutineEnv_t *env = co->env;
    stCoRoutine_t *lpCurrRoutine = env->pCallStack[env->iCallStackSize - 1];
    if (!co->cStart) {
        coctx_make(&co->ctx, (coctx_pfn_t)CoRoutineFunc, co, 0);
        co->cStart = 1;
    }
    env->pCallStack[env->iCallStackSize++] = co;
    co_swap(lpCurrRoutine, co);
}


void co_yield_ct()
{
    co_yield_env(co_get_curr_thread_env());
}


// ----------------------------- co_poll -------------------------
struct stPollItem_t;
struct stPoll_t : public stTimeoutItem_t {
    nfds_t nfds;
    stPollItem_t *pPollItems;	// array, length nfds
    int iAllEventDetach;
    int iRaiseCnt;
};

struct stPollItem_t {
    short revents;
    struct epoll_event stEvent;

    stPoll_t *pPoll;
};

struct stCoEpoll_t {
    static const int _EPOLL_SIZE = 1024 * 10;

    int iEpollFd;
    uint64_t epollNow;
    stTimeout_t *pTimeout;
    stTimeoutItemLink_t *pstActiveList;
};

/*
 *   EPOLLPRI 		POLLPRI    // There is urgent data to read.  
 *   EPOLLMSG 		POLLMSG
 *
 *   				POLLREMOVE
 *   				POLLRDHUP
 *   				POLLNVAL
 *
 * */
static uint32_t PollEvent2Epoll(short events)
{
    uint32_t e = 0;
    if (events & POLLIN)
        e |= EPOLLIN;
    if (events & POLLOUT)
        e |= EPOLLOUT;
    if (events & POLLHUP)
        e |= EPOLLHUP;
    if (events & POLLERR)
        e |= EPOLLERR;
    if (events & POLLRDNORM)
        e |= EPOLLRDNORM;
    if (events & POLLWRNORM)
        e |= EPOLLWRNORM;
    return e;
}

static short EpollEvent2Poll(uint32_t events)
{
    short e = 0;
    if (events & EPOLLIN)
        e |= POLLIN;
    if (events & EPOLLOUT)
        e |= POLLOUT;
    if (events & EPOLLHUP)
        e |= POLLHUP;
    if (events & EPOLLERR)
        e |= POLLERR;
    if (events & EPOLLRDNORM)
        e |= POLLRDNORM;
    if (events & EPOLLWRNORM)
        e |= POLLWRNORM;
    return e;
}

static void OnPollPreparePfn(stPollItem_t *lp, struct epoll_event &e, stTimeoutItemLink_t *active)
{
    lp->revents = EpollEvent2Poll(e.events);

    stPoll_t *pPoll = lp->pPoll;
    pPoll->iRaiseCnt++;

    if (!pPoll->iAllEventDetach) {
        pPoll->iAllEventDetach = 1;
        RemoveFromLink<stTimeoutItem_t, stTimeoutItemLink_t>(pPoll);
        AddTail(active, pPoll);
    }
}

void co_eventloop(stCoEpoll_t *ctx, pfn_co_eventloop_t pfn, void *arg)
{
    co_epoll_res *result = co_epoll_res_alloc(stCoEpoll_t::_EPOLL_SIZE);;
    stTimeoutItemLink_t *timeout = (stTimeoutItemLink_t *)malloc(sizeof(stTimeoutItemLink_t));
    stTimeoutItemLink_t *active = ctx->pstActiveList;

    for (;;) {
        // add epoll event
        int ret = co_epoll_wait(ctx->iEpollFd, result, stCoEpoll_t::_EPOLL_SIZE, 1);
        for (int i = 0; i < ret; i++) {
            stPollItem_t *item = (stPollItem_t *)result->events[i].data.ptr;
            OnPollPreparePfn(item, result->events[i], active);
        }

        // add timeout event
        uint64_t now = GetTickNow();
        ctx->epollNow = now;
        memset(timeout, 0, sizeof(stTimeoutItemLink_t));
        ctx->pTimeout->takeAllTimeout(now, timeout);
        stTimeoutItem_t *lp = timeout->head;
        while (lp) {
            stTimeoutItem_t *nt = lp->pNext;
            if (now < lp->expireTime) {
                RemoveFromLink<stTimeoutItem_t, stTimeoutItemLink_t>(lp);
                ctx->pTimeout->addTimeout(lp, now);
            }
            lp = nt;
        }
        Join<stTimeoutItem_t, stTimeoutItemLink_t>(active, timeout);

        // callback user function
        if (pfn) {
            if (pfn(arg) < 0) {
                break;
            }
        }

        lp = active->head;
        while (lp) {
            PopHead<stTimeoutItem_t, stTimeoutItemLink_t>(active);

            bool autoFree = lp->bAutoFree;
            if (lp->func)
                lp->func(lp->arg);
            if (autoFree)
                free(lp);

            lp = active->head;
        }
    }

    co_epoll_res_free(result);
    free(timeout);
}

stCoEpoll_t *AllocEpoll()
{
    stCoEpoll_t *ctx = (stCoEpoll_t *)calloc(1, sizeof(stCoEpoll_t));
    ctx->iEpollFd = co_epoll_create(stCoEpoll_t::_EPOLL_SIZE);
    ctx->epollNow = GetTickNow();
    ctx->pTimeout = new stTimeout_t;
    ctx->pstActiveList = (stTimeoutItemLink_t *)calloc(1, sizeof(stTimeoutItemLink_t));
    return ctx;
}

void co_async_call(stCoEpoll_t *ctx, pfn_co_call_t func, void *arg)
{
    stTimeoutItem_t *timeout = (stTimeoutItem_t*)calloc(sizeof(stTimeoutItem_t), 1);
    timeout->func = func;
    timeout->arg = arg;
    timeout->bAutoFree = true;
    AddTail(ctx->pstActiveList, timeout);
}

typedef int (*poll_pfn_t)(struct pollfd fds[], nfds_t nfds, int timeout);
int co_poll_inner(stCoEpoll_t *ctx, struct pollfd fds[], nfds_t nfds, int timeout, poll_pfn_t pollfunc)
{
    if (timeout == 0) {
        return pollfunc(fds, nfds, timeout);
    }
    if (timeout < 0) {
        timeout = INT_MAX;
    }
    int epfd = ctx->iEpollFd;

    //1.struct change
    stPoll_t arg;
    memset(&arg, 0, sizeof(arg));
    arg.nfds = nfds;
    stPollItem_t arr[2];
    if (nfds < sizeof(arr) / sizeof(stPollItem_t)) {
        arg.pPollItems = arr;
    } else {
        arg.pPollItems = (stPollItem_t *)malloc(nfds * sizeof(stPollItem_t));
    }
    memset(arg.pPollItems, 0, nfds * sizeof(stPollItem_t));
    arg.func = (pfn_co_call_t)co_resume;
    arg.arg = GetCurrCo(co_get_curr_thread_env());

    //2. add epoll
    for (nfds_t i = 0; i < nfds; i++) {
        arg.pPollItems[i].pPoll = &arg;
        struct epoll_event &ev = arg.pPollItems[i].stEvent;

        if (fds[i].fd > -1) {
            ev.data.ptr = arg.pPollItems + i;
            ev.events = PollEvent2Epoll(fds[i].events);

            int ret = co_epoll_ctl(epfd, EPOLL_CTL_ADD, fds[i].fd, &ev);
            if (ret < 0 && errno == EPERM && nfds == 1 && pollfunc != NULL) {
                if (arg.pPollItems != arr) {
                    free(arg.pPollItems);
                    arg.pPollItems = NULL;
                }
                return pollfunc(fds, nfds, timeout);
            }
        }
        //if fail,the timeout would work
    }

    //3.add timeout
    arg.expireTime = ctx->epollNow + timeout;
    ctx->pTimeout->addTimeout(&arg, ctx->epollNow);

    int iRaiseCnt = 0;
    co_yield_env(co_get_curr_thread_env());
    iRaiseCnt = arg.iRaiseCnt;

    {
        //clear epoll status and memory
        RemoveFromLink<stTimeoutItem_t, stTimeoutItemLink_t>(&arg);
        for (nfds_t i = 0; i < nfds; i++) {
            int fd = fds[i].fd;
            if (fd > -1) {
                co_epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &arg.pPollItems[i].stEvent);
            }
            fds[i].revents = arg.pPollItems[i].revents;
        }

        if (arg.pPollItems != arr) {
            free(arg.pPollItems);
            arg.pPollItems = NULL;
        }
    }

    return iRaiseCnt;
}

int co_poll(stCoEpoll_t *ctx, struct pollfd fds[], nfds_t nfds, int timeout_ms)
{
    return co_poll_inner(ctx, fds, nfds, timeout_ms, NULL);
}

stCoEpoll_t *co_get_epoll_ct()
{
    if (!co_get_curr_thread_env()) {
        co_init_curr_thread_env();
    }
    return co_get_curr_thread_env()->pEpoll;
}

void *co_getspecific(pthread_key_t key)
{
    stCoRoutine_t *co = co_self();
    if (!co || co->cIsMain) {
        return pthread_getspecific(key);
    }
    return co->aSpec[key].value;
}

int co_setspecific(pthread_key_t key, const void *value)
{
    stCoRoutine_t *co = co_self();
    if (!co || co->cIsMain) {
        return pthread_setspecific(key, value);
    }
    co->aSpec[key].value = (void *)value;
    return 0;
}

stCoRoutine_t *co_self()
{
    stCoRoutineEnv_t *env = co_get_curr_thread_env();
    if (!env)
        return 0;
    return GetCurrCo(env);
}

//co cond
struct stCoCond_t;
struct stCoCondItem_t {
    stCoCondItem_t *pPrev;
    stCoCondItem_t *pNext;
    stCoCond_t *pLink;

    stTimeoutItem_t timeout;
};

struct stCoCond_t {
    stCoCondItem_t *head;
    stCoCondItem_t *tail;
};

stCoCondItem_t *co_cond_pop(stCoCond_t *link)
{
    stCoCondItem_t *p = link->head;
    if (p) {
        PopHead<stCoCondItem_t, stCoCond_t>(link);
    }
    return p;
}

int co_cond_signal(stCoCond_t *si)
{
    stCoCondItem_t *sp = co_cond_pop(si);
    if (!sp) {
        return 0;
    }
    RemoveFromLink<stTimeoutItem_t, stTimeoutItemLink_t>(&sp->timeout);
    AddTail(co_get_curr_thread_env()->pEpoll->pstActiveList, &sp->timeout);
    return 0;
}

int co_cond_broadcast(stCoCond_t *si)
{
    for (;;) {
        stCoCondItem_t *sp = co_cond_pop(si);
        if (!sp)
            return 0;
        RemoveFromLink<stTimeoutItem_t, stTimeoutItemLink_t>(&sp->timeout);
        AddTail(co_get_curr_thread_env()->pEpoll->pstActiveList, &sp->timeout);
    }
    return 0;
}

int co_cond_timedwait(stCoCond_t *link, int ms)
{
    stCoCondItem_t si;
    memset(&si, 0, sizeof(si));
    stCoCondItem_t *psi = &si;
    psi->timeout.arg = GetCurrCo(co_get_curr_thread_env());
    psi->timeout.func = (pfn_co_call_t)co_resume;

    if (ms > 0) {
        stCoEpoll_t *ctx = co_get_curr_thread_env()->pEpoll;
        psi->timeout.expireTime = ctx->epollNow + ms;
        ctx->pTimeout->addTimeout(&psi->timeout, ctx->epollNow);
    }
    AddTail(link, psi);

    co_yield_ct();

    RemoveFromLink<stCoCondItem_t, stCoCond_t>(psi);
    return 0;
}

stCoCond_t *co_cond_alloc()
{
    return (stCoCond_t *)calloc(1, sizeof(stCoCond_t));
}

int co_cond_free(stCoCond_t *cc)
{
    free(cc);
    return 0;
}
