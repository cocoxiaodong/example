/**
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>


// 列表前插
#define LL_ADD(item, list) do {             \
    item->prev = NULL;                      \
    item->next = list;                      \
    if (list != NULL) list->prev = item;    \
    list = item;                             \
} while (0)


// 列表任何位置删除
#define LL_REMOVE(item, list) do {          \
    if (item->prev != NULL) item->prev->next = item->next;  \
    if (item->next != NULL) item->next->prev = item->prev;  \
    if (item == list) list = item->next;                    \
    item->prev = item->next = NULL;                         \
} while (0)


// 任务队列
struct NJOB
{
    void (*func)(void *arg);
    void *user_data;

    struct NJOB *prev;
    struct NJOB *next;
};

// 工作队列
struct NWORKER
{
    pthread_t id;
    struct NMANAGER *pool;
    int terminate;  // 终止

    struct NWORK *prev;
    struct NWORK *next;
};

// 管理组件
typedef struct NMANAGER
{
    pthread_mutex_t mtx;
    pthread_cond_t cond;

    struct NJOB *jobs;
    struct NWORKER *workers;
} nThreadPool;

static void* nTheadCallBack(void* arg)
{
    struct NWORKER *worker = (struct NWORKER*)arg;

    while(1)
    {
        pthread_mutex_lock(&worker->pool->mtx);
        while(worker->pool->jobs == NULL)
        {
            if (worker->terminate) break;
            pthread_cond_wait(&worker->pool->cond, &worker->pool->mtx);
        }
        struct NJOB *job = worker->pool->jobs;
        if (job != NULL)
        {
            LL_REMOVE(job, worker->pool->jobs);
        }
        pthread_mutex_unlock(&worker->pool->mtx);
        job->func(job);
    }
}

int nThreadPoolCreate(nThreadPool *pool, int numWorkers)
{
    if (numWorkers < 1) numWorkers = 1;
    if (pool == NULL) return -1;
    memset(pool, 0, sizeof(nThreadPool));

    pthread_mutex_t blank_mutex = PTHREAD_MUTEX_INITIALIZER;
    memcpy(&pool->mtx, &blank_mutex, sizeof(pthread_mutex_t));
    //
    pthread_cond_t blank_cond = PTHREAD_MUTEX_INITIALIZER;
    memcpy(&pool->cond, &blank_cond, sizeof(pthread_cond_t));

    // works
    int i = 0;
    for (i = 0; i < numWorkers; ++i)
    {
        struct NWORKER *worker = (struct NWORKER*)malloc(sizeof(struct NWORKER));
        if (worker == NULL)
        {
            perror("malloc");
            return -2;
        }
        memset(worker, 0, sizeof(struct NWORKER));
        worker->pool = pool; //
        //
        pthread_create(&worker->id, NULL, nTheadCallBack, worker);
        LL_ADD(worker, pool->workers);
    }
    return 0;
}

int nThreadPoolPushTask(nThreadPool *pool, struct NJOB* job)
{
    pthread_mutex_lock(&pool->mtx);

    LL_ADD(job, pool->jobs);
    pthread_cond_signal(&pool->cond);

    pthread_mutex_unlock(&pool->mtx);

    return 0;
}

int nThreadPoolDestory(nThreadPool *pool)
{
    if (pool == NULL) return -1;
    
    struct NWORKER *node = pool->workers;

    // 通知所有线程去自杀，在自己领取任务过程当中
    //while (node != NULL)
    //{
    //    pthread_cond_broadcast(&pool->cond);
    //    
    //    node = node->next;
    //}

    // /*等待线程结束 先是pthread_exit 然后等待其结束*/
    while (node != NULL)
    {
        pthread_mutex_lock(&pool->mtx);
        node->terminate = 1;
        pthread_join(node->id, NULL);
        struct NWORKER *del_node = node;
        node = node->next;
        free(del_node);
        pthread_mutex_unlock(&pool->mtx);
    }

    pthread_mutex_destroy(&pool->mtx);
    
    pool = NULL;
 
    return 0;
}

// sdk
//nThreadPoolCreate
//nThreadPoolPushTask
//nThreadPoolDestory

#if 1

pthread_mutex_t data_mtx;
struct DATA
{
    int num;
};

void test_call_back(struct NJOB* job)
{
    pthread_mutex_lock(&data_mtx);
    struct DATA *data = (struct DATA*)(job->user_data);
    data->num++;
    pthread_mutex_unlock(&data_mtx);
}

int main ()
{
    printf("begin test \n");
    nThreadPool thread_pool;
    nThreadPoolCreate(&thread_pool, 10);
    //printf("create thread pool ok \n");

    struct DATA *data = (struct DATA*)malloc(sizeof(struct DATA));
    memset(data, 0, sizeof(struct DATA));

    int i = 0;
    for (i = 0; i < 1000; ++i)
    {
        struct NJOB *job = (struct NJOB*)malloc(sizeof(struct NJOB));
        memset(job, 0, sizeof(struct NJOB));

        job->func = test_call_back;
        job->user_data = data;
        nThreadPoolPushTask(&thread_pool, job);
        //printf("create job %d \n", i);
    }
    sleep(2); // 因为异步操作，线程池还在工作
    printf("result num = %d \n", data->num);
    nThreadPoolDestory(&thread_pool);
    return 0;
}
#endif
