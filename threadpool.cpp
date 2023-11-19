#include "threadpool.h"
#include <functional>
#include <thread>
#include <iostream>
const int MAX_SIZE=1024;
const int THREAD_MAX_THRESHHOLD=10;
const int THREAD_MAX_IDLE_TIME=60;  //单位：秒
int Thread::generateId_=0;

ThreadPool::ThreadPool()
:initThreadSize_(0),
taskSize_(0),
taskQueMaxThreshHold_(MAX_SIZE),
poolMode_(PoolMode::MODE_FIXED),
isPoolRunning_(false),
idleThreadSize_(0),
threadSizeThreshHold_(THREAD_MAX_THRESHHOLD),
curThreadSize_(0)
{}

ThreadPool::~ThreadPool()
{
    isPoolRunning_= false;
    notEmpty_.notify_all();
    //等待所有线程池线程返回
    std::unique_lock<std::mutex> lock(taskQueMtx_);
    exitCond_.wait(lock,[&](){
        return threads_.size()==0;
    });
}

void ThreadPool::start(int size) {      //开始创建线程
    //设置线程池的运行状态
    isPoolRunning_=true;
    //记录线程个数
    initThreadSize_=size;
    curThreadSize_=size;
    for(int i=0;i<initThreadSize_;i++){
        auto p=std::make_unique<Thread>(
                std::bind(&ThreadPool::threadFunc,this,std::placeholders::_1));
        int threadId=p->getId();
        threads_.emplace(threadId,std::move(p));
        //        threads_.push_back(std::move(p));
    }
    for(int i=0;i<initThreadSize_;i++){
        threads_[i]->start();   //需要去执行一个线程函数
        idleThreadSize_++;  //记录初始空闲线程的数量
    }
}

void ThreadPool::threadFunc(int threadid){  //这里没按照视频里来做的，如果出问题，请参考视频15集
    auto lastTime=std::chrono::high_resolution_clock().now();
    //必须等所有任务都执行完，线程才能结束
    while(true)
    {
        //获取锁
        std::cout<<std::this_thread::get_id()<<"开始抢任务"<<std::endl;
        std::unique_lock<std::mutex> lock(taskQueMtx_);

        while(taskQue_.size()==0)
        {
            //线程池要结束，回收资源
            if(!isPoolRunning_)
            {
                threads_.erase(threadid);
                std::cout<<"threadid:"<<std::this_thread::get_id()<<"exit"<<std::endl;
                exitCond_.notify_all();
                return;
            }
            if(poolMode_==PoolMode::MODE_CACHED)
            {
                //条件变量，超时返回了
                //每一秒返回一次
                //cached模式下，有可能已经创建了很多的线程，但是空闲时间超过60s，应该把多余的线程回收掉
                //回收超过initThreadSize_数量的线程
                //当前时间-上一次执行的时间
                //线程通信
                if(std::cv_status::timeout==notEmpty_.wait_for(lock,std::chrono::seconds(1)))
                {
                    auto now=std::chrono::high_resolution_clock ().now();
                    auto dur=std::chrono::duration_cast<std::chrono::seconds>(now-lastTime);
                    if(dur.count()>=THREAD_MAX_IDLE_TIME&&curThreadSize_>initThreadSize_)
                    {
                        //开始回收当前线程
                        //记录线程数量的相关的值要修改
                        //把线程对象从线程列表容器中删除   （很难）
                        threads_.erase(threadid);
                        curThreadSize_--;
                        idleThreadSize_--;
                        std::cout<<"threadid:"<<std::this_thread::get_id()<<"exit"<<std::endl;
                        return;
                    }
                }
            }
            else
            {
                notEmpty_.wait(lock);
            }
        }

        idleThreadSize_--;  //空闲线程数量减一
        //线程去任务队列中获取任务
//        std::cout<<std::this_thread::get_id()<<"任务抢夺成功"<<std::endl;
        auto task=taskQue_.front();
        taskQue_.pop();
        taskSize_--;
        lock.unlock();
        //通过notFull进行线程通信，“昭告天下”
        notFull_.notify_all();
        //任务执行
//        task->run();
        task->exec();
        idleThreadSize_++;  //空闲线程数量加一
        lastTime=std::chrono::high_resolution_clock().now();    //更新
        std::cout<<std::this_thread::get_id()<<"任务执行成功"<<std::endl;
    }
}

//void ThreadPool::threadFunc(){  //这里按照视频里来做的
//
//    for(;;) {
//        std::shared_ptr<Task> ptr;
//        {
//            //获取锁
//            std::cout <<std::this_thread::get_id()<< "开始抢任务" << std::endl;
//            std::unique_lock<std::mutex> lock(taskQueMtx_);
//            //线程通信
//            notEmpty_.wait(lock, [&] {
//                if (taskQue_.size() == 0)
//                    return false;
//                return true;
//            });
//            //线程去任务队列中获取任务
////            std::cout <<std::this_thread::get_id()<< "任务抢夺成功" << std::endl;
////        auto task=taskQue_.front();
//            ptr = taskQue_.front();
//            taskQue_.pop();
//            taskSize_--;
//            if(taskQue_.size()>0)
//                notEmpty_.notify_all();
//            //通过notFull进行线程通信，“昭告天下”
//            notFull_.notify_all();
//            //任务执行
//        }
//        if(ptr!= nullptr){
//            ptr->run();
//            std::cout<<std::this_thread::get_id()<<"任务执行成功"<<std::endl;
//        }
//    }
//
//}

void ThreadPool::setMode(PoolMode mode) {   //设置模式
    if(checkRunningState())
    {
        return;
    }
    poolMode_=mode;
}

void ThreadPool::setTaskQueMaxThreshHold(int threshhold) {  //设置任务队列数量最大值
    if(checkRunningState())
    {
        return;
    }
    taskQueMaxThreshHold_=threshhold;
}

void ThreadPool::setThreadSizeThreshHold(int threshhold){
    if(checkRunningState())
    {
        return;
    }
    if(poolMode_==PoolMode::MODE_CACHED) {
        threadSizeThreshHold_ = threshhold;
    }
}

Result ThreadPool::submitTask(std::shared_ptr<Task> sp) { //用户提交任务
    //获取锁
    std::unique_lock<std::mutex> lock(taskQueMtx_);
    //线程通信
    //用户提交任务，如果等待时间超过了1s，则显示提交失败，返回
    if(!notFull_.wait_for(lock,std::chrono::seconds(1),[&]{
            if(taskQue_.size()==taskQueMaxThreshHold_)
                return false;
             return true;
    })){
        std::cout<<"任务提交失败"<<std::endl;
        return Result(sp, false);
    }
    //将任务放进任务队列中
    taskQue_.emplace(sp);
    taskSize_++;
    //通过notEmpty进行通知
    notEmpty_.notify_all();
    //如果是cached模式，任务比较紧急，
    if(poolMode_==PoolMode::MODE_CACHED
    &&taskSize_>idleThreadSize_
    &&curThreadSize_<threadSizeThreshHold_)
    {
        //创建新线程
        auto p=std::make_unique<Thread>(std::bind(
                &ThreadPool::threadFunc,this,std::placeholders::_1));
        int threadId=p->getId();
        threads_.emplace(threadId,std::move(p));
        curThreadSize_++;
        //启动
        threads_[threadId]->start();
        //修改线程相关变量
        curThreadSize_++;
        idleThreadSize_++;
    }
    return Result(sp);
}

Thread::Thread(Thread::ThreadFunc func)
:func_(func),threadId_(generateId_++)
{}

Thread::~Thread() {}

void Thread::start() {  //线程类创建一个永久运行的线程
    std::thread t(func_,threadId_);
    t.detach();
}

bool ThreadPool::checkRunningState()const{
    return isPoolRunning_;
}

int Thread::getId()const{
    return threadId_;
}







Result::Result(std::shared_ptr<Task> task, bool isValid)
:isValid_(isValid)
,task_(task)
{
    task_->setResult(this);
}

Any Result::get(){
    if(!isValid_){
        return "";
    }
    sem_.wait();    //task任务如果没有执行完，这里会阻塞用户的线程
    return std::move(any_);
}

void Result::setVal(Any any){
    this->any_=std::move(any);
    sem_.post();    //已经获取人物的返回值，增加信号量资源
}

void Task::exec() {
    if(result_!= nullptr){
        result_->setVal(run()); //这里发生多态
    }
}

void Task::setResult(Result *res) {
    result_=res;
}

Task::Task() :result_(nullptr){}
