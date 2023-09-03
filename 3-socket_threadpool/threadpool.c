#include "threadpool.h"
#include <pthread.h>
#include <unistd.h> //使用usleep需要
#include <stdlib.h> //malloc
#include <stdio.h>
#include <string.h> //memset


const int CHANGETHREADNUM = 2;

//任务结构体
typedef struct Task
{
    void (*function) (void* arg); 
    void* arg;
}Task;  //c++的话不加typedef就可以直接使用Task，但是c不行

//线程池结构体
struct ThreadPool
{
    //任务队列
    Task* taskQ;
    int queueCapacity;  //容量
    int queueSize;      //当前任务个数
    int queueFront;     //队头-》队尾
    int queueRear;      //队尾-》队头

    pthread_t managerID;    //管理线程ID
    pthread_t* threadIDs;   //工作线程ID，多个
    int minNum;     //线程数量min
    int maxNum;
    int busyNum;    //当前正忙线程数——此变量会经常变化
    int liveNum;    //当前存活线程（包含正忙）
    int exitNum;    //需要销毁线程数

    pthread_mutex_t mutexpool; //锁-整个线程池
    pthread_mutex_t mutexBusy; //锁busyNum变量-因为该变量经常访问使用

    int shutdown;               //是否要销毁线程池，销毁为1，不销毁为0

    pthread_cond_t notFull;     //任务队列是否满了
    pthread_cond_t notEmpty;
};

ThreadPool* threadPoolCreate(int min, int max, int queueSize)
{
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));

    do
    {
        if (pool == NULL) {
            printf("malloc thread fail...\n");
            break;
        }
        pool->threadIDs = (pthread_t*)malloc(sizeof(pthread_t) * max); 

        if (pool->threadIDs == NULL) {
            printf("malloc threadIDs fail...\n");
            break;
        }

        memset(pool->threadIDs, 0, sizeof(pthread_t) * max); //辅助判断，为0则该线程没有使用

        pool->minNum = min;
        pool->maxNum = max;
        pool->busyNum = 0;
        pool->liveNum = min;    //和最小个数相同
        pool->exitNum = 0;


        if (pthread_mutex_init(&pool->mutexpool, NULL) != 0 ||
            pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
            pthread_cond_init(&pool->notFull, NULL) != 0 ||  //初始化失败
            pthread_cond_init(&pool->notEmpty, NULL) != 0)
        {
            printf("init mutex or cond fail...\n");
        }

        //任务队列
        pool->taskQ = (Task*)malloc(sizeof(Task) * queueSize);
        pool->queueCapacity = queueSize;
        pool->queueSize = 0;
        pool->queueFront = 0;
        pool->queueRear = 0;

        pool->shutdown = 0;

        //创建线程
        pthread_create(&pool->managerID, NULL, manager, pool); //
        for (int i = 0; i < min; i++) {
            pthread_create(&pool->threadIDs[i], NULL, worker, pool);
        }
        return pool;
    } while (0);
    
    //break了就进行资源的释放
    if (pool && pool->threadIDs) free(pool->threadIDs);
    if (pool->taskQ) free(pool->taskQ);
    if (pool) free(pool);
    
    return NULL;
}

int threadPoolDestory(ThreadPool* pool)
{
    if (pool == NULL) return -1;
    //先关闭
    pool->shutdown = 1;  //之后管理者判断shutdown为1 会退出
    //阻塞回收管理者线程
    pthread_join(pool->managerID, NULL);
    for (int i = 0; i < pool->liveNum; ++i) {
        pthread_cond_signal(&pool->notEmpty); //这里会转到6.2，促使线程自杀
    }
    //释放堆内存
    if (pool->taskQ) free(pool->taskQ);
    if (pool->threadIDs) free(pool->threadIDs);
    pthread_mutex_destroy(&pool->mutexpool);
    pthread_mutex_destroy(&pool->mutexBusy);
    pthread_cond_destroy(&pool->notEmpty);
    pthread_cond_destroy(&pool->notFull);

    free(pool);
    pool = NULL;

    return 0;
}

void threadPoolAdd(ThreadPool* pool, void(*func)(void*), void* arg)
{
    pthread_mutex_lock(&pool->mutexpool);
    while (pool->queueSize == pool->queueCapacity && !pool->shutdown) //满了，但是还在使用
    {
        //阻塞生产者
        pthread_cond_wait(&pool->notFull, &pool->mutexpool);
    }
    if (pool->shutdown)
    {
        pthread_mutex_unlock(&pool->mutexpool);
        return;
    }
    //添加任务
    pool->taskQ[pool->queueRear].function = func;
    pool->taskQ[pool->queueRear].arg = arg;
    pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
    //任务个数
    pool->queueSize++;

    pthread_cond_signal(&pool->notEmpty);
    pthread_mutex_unlock(&pool->mutexpool);
}

int threadPoolBusyNum(ThreadPool* pool)
{
    int busyNum = 0;
    pthread_mutex_lock(&pool->mutexBusy);
    busyNum = pool->busyNum;
    pthread_mutex_unlock(&pool->mutexBusy);
    return busyNum;
}

int threadPoolAliveNum(ThreadPool* pool)
{
    int aliveNum = 0;
    pthread_mutex_lock(&pool->mutexpool);
    aliveNum = pool->liveNum;
    pthread_mutex_unlock(&pool->mutexpool);
    return aliveNum;
}

void* worker(void* arg)
{
    ThreadPool* pool = (ThreadPool*)arg;
    while (1)
    {
        //一直读取任务队列
        pthread_mutex_lock(&pool->mutexpool);
        //当前队列是否为空
        while (pool->queueSize == 0 && !pool->shutdown)
        {
            //阻塞工作线程
            pthread_cond_wait(&pool->notEmpty, &pool->mutexpool);

            //5.1判断是否需要销毁线程
            if (pool->exitNum > 0) {
                pool->exitNum--;
                if (pool->liveNum > pool->minNum)
                {
                    pool->liveNum--;
                    pthread_mutex_unlock(&pool->mutexpool);
                    threadExit(pool);

                }
            }
        }
        // 判断当前线程池是否关闭
        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->mutexpool);
            threadExit(pool);  //6.2
        }

        // 从任务队列中取出一个任务
        Task task;
        task.function = pool->taskQ[pool->queueFront].function; //这里是一个数组，所以按照index取用
        task.arg = pool->taskQ[pool->queueFront].arg;
        //移动头结点
        pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
        pool->queueSize--;
        //解锁
        pthread_cond_signal(&pool->notFull);
        pthread_mutex_unlock(&pool->mutexpool);

        printf("thread %ld start working..\n", pthread_self());
        //执行前，busy++
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum++;
        pthread_mutex_unlock(&pool->mutexBusy);

        //执行任务
        // task.function(task.arg);  //这里是通过函数指针的方式进行调用，还可以对函数指针先进行解引用，然后函数调用
        (*task.function)(task.arg);
        //建议传参为堆内存——保证在线程执行期间可以访问到
        free(task.arg);
        task.arg = NULL;

        printf("thread %ld end working..\n", pthread_self());
        //执行完毕，busy--
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum--;
        pthread_mutex_unlock(&pool->mutexBusy);
    }
    return NULL;
}

void* manager(void* arg)
{
    ThreadPool* pool = (ThreadPool*)arg;
    while (!pool->shutdown)
    {
        //每隔3秒检测一次
        sleep(3);
        //取出任务数量和当前线程数量
        pthread_mutex_lock(&pool->mutexpool);
        int queueSize = pool->queueSize;
        int liveNum = pool->liveNum;
        pthread_mutex_unlock(&pool->mutexpool);

        //取出忙的线程的数量
        pthread_mutex_lock(&pool->mutexBusy);
        int busyNum = pool->busyNum; // 这里也可以一起放在上面，因为整个pool都会被锁定——但我觉得不对，万一前面没有mutexpool，但是锁定了mutexBusy了呢
        pthread_mutex_unlock(&pool->mutexBusy);

        //添加线程
        //执行规则：任务的个数>存活的线程个数-busy && 存活的线程数<最大线程数，每次添加2(ADDTHREADNUMBERPERTIME)个——可以改ccc
        if (queueSize > liveNum-busyNum && liveNum < pool->maxNum)
        {
            int counter = 0;
            pthread_mutex_lock(&pool->mutexpool);
            for (int i = 0; i < pool->maxNum      //循环整个数组
                    && counter < CHANGETHREADNUM     //但是只增加ADDTHREADNUMBERPERTIME次
                    && pool->liveNum < pool->maxNum;        //防止中间添加一半就满了
                    i++) //
            {
                if (pool->threadIDs[i] == 0) //转到6.1 要求线程执行完毕后，threadID就变成0
                {
                    pthread_create(&pool->threadIDs[i], NULL, worker, pool);
                    counter++;
                    pool->liveNum++;

                }
            }
            pthread_mutex_unlock(&pool->mutexpool);
        }

        //销毁线程
        //执行规则：忙的线程*2 < 存活的线程个数 && 存活的线程数>最小线程数，每次添加2(ADDTHREADNUMBERPERTIME)个
        if (busyNum * 2 < liveNum && liveNum > pool->minNum) { //pool->minNum没有加锁：因为设定好只读
            pthread_mutex_lock(&pool->mutexpool);
            pool->exitNum = CHANGETHREADNUM;
            pthread_mutex_unlock(&pool->mutexpool);

            //让工作的线程自杀
            for (int i = 0; i < CHANGETHREADNUM; ++i) {
                pthread_cond_signal(&pool->notEmpty);  //braodcast也是一样，每次只会有一个线程抢到
                //转到5.1
            }

        }
    }
    return NULL;
}

void threadExit(ThreadPool* pool)
{
    pthread_t tid = pthread_self();
    for (int i = 0; i < pool->maxNum; ++i) {
        if (pool->threadIDs[i] == tid)
        {
            pool->threadIDs[i] = 0;
            printf("threadExit() called, %ld exiting...\n", tid);
            break;
        }
    }
    pthread_exit(NULL);
}
