#pragma once 

#include <iostream>
#include <queue>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <sys/stat.h>
#include <mutex>
#include "utils.hpp"

typedef bool (*Hand) (int sock);
class HttpTask {
  private:
    int _cli_sock;
    Hand _handler;
  public:
    HttpTask(int cli_sock = -1,Hand handler = NULL)
      :_cli_sock(cli_sock)
       ,_handler(handler)
    {
      
    }
    void Handler()
    {
         _handler(_cli_sock);
    }
    ~HttpTask()
    {}

};	

class ThreadPool
{
   //线程池类
   //创建指定数量的线程
  //  创建一个线程安全的任务队列
  //  提供任务的入队，出队，线程池销毁/初始化接口
     private:
            int _max_num;  //当前线程池的最大线程数
             std::queue<HttpTask> _task_queue;
             pthread_mutex_t  _mutex;
             pthread_cond_t _cond;
            int _cur_num; // 当前线程池的线程数
            bool _is_stop;
   public:
            ThreadPool(int max_num = 5)
              :_max_num(max_num) 
               ,_cur_num(0)
               ,_is_stop(false)
           {
           }
             ~ThreadPool()
             {
               pthread_mutex_destroy(&_mutex);
               pthread_cond_destroy(&_cond);
             }
             static void *PthreadRoutine(void *arg)
             {
                    ThreadPool *tp = (ThreadPool*)arg;
                    pthread_detach(pthread_self());
                    for(;;)
                    {
                      tp->LockQueueTask();
                      while(tp->TaskQueueIsEmpty()) //如果任务有多个，一直让它处理完
                        tp->ThreadWait();
                      HttpTask t;
                      tp->PopTask(t);
                      tp->UnlockQueueTask();
                      t.Handler();

                   }
             }
            bool ThreadPoolInit() // 完成线程创建，互斥锁，条件变量
            {
                pthread_mutex_init(&_mutex,NULL);
                pthread_cond_init(&_cond,NULL);

                pthread_t tid[_max_num];
                for(int i = 0; i < _max_num; i++)
                {
                  int ret = pthread_create(&tid[i],NULL,PthreadRoutine,this);
                      if(ret != 0)
                      {
                        std::cout << "thread create error" << std::endl;
                        return false;
                      }
                      _cur_num++;
                }
                      return true;

            }
            bool PushTask(HttpTask &tt) //线程安全的任务入队
            {
              LockQueueTask();
              _task_queue.push(tt);
              WakeOneThread();
              UnlockQueueTask();
              return true;
            }
            bool PopTask(HttpTask &tt)  //线程安全的任务出队
            {
              //在出队时已经加锁过了
              tt = _task_queue.front();
              _task_queue.pop();
               return true;
            }
            bool ThreadPoolStop() //销毁线程池 
            {
              LockQueueTask();
              _is_stop = true;
              while(_cur_num > 0)
                WakeAllThread();
                return true;
            }
   private:
            void ThreadWait()
            {
              if(_is_stop)
              {
                UnlockQueueTask();
                _cur_num--;
                pthread_exit(NULL);
              }
              pthread_cond_wait(&_cond,&_mutex);

            }
     bool TaskQueueIsEmpty()
  
    {
        return _task_queue.empty();
  
    }
    void LockQueueTask()
    {
      
  pthread_mutex_lock(&_mutex);
 
   }
    void UnlockQueueTask()
    {
 
       pthread_mutex_unlock(&_mutex);
   
 }
    void WakeOneThread()
    {
    
   pthread_cond_signal(&_cond);

   }
    void WakeAllThread()
    {
      
  pthread_cond_broadcast(&_cond);
   
  }
};
