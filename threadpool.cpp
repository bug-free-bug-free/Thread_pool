#include "threadpool.h"
#include <functional>
#include <thread>
#include <iostream>
const int MAX_SIZE=1024;
const int THREAD_MAX_THRESHHOLD=10;
const int THREAD_MAX_IDLE_TIME=60;  //��λ����
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
    //�ȴ������̳߳��̷߳���
    std::unique_lock<std::mutex> lock(taskQueMtx_);
    exitCond_.wait(lock,[&](){
        return threads_.size()==0;
    });
}

void ThreadPool::start(int size) {      //��ʼ�����߳�
    //�����̳߳ص�����״̬
    isPoolRunning_=true;
    //��¼�̸߳���
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
        threads_[i]->start();   //��Ҫȥִ��һ���̺߳���
        idleThreadSize_++;  //��¼��ʼ�����̵߳�����
    }
}

void ThreadPool::threadFunc(int threadid){  //����û������Ƶ�������ģ���������⣬��ο���Ƶ15��
    auto lastTime=std::chrono::high_resolution_clock().now();
    //�������������ִ���꣬�̲߳��ܽ���
    while(true)
    {
        //��ȡ��
        std::cout<<std::this_thread::get_id()<<"��ʼ������"<<std::endl;
        std::unique_lock<std::mutex> lock(taskQueMtx_);

        while(taskQue_.size()==0)
        {
            //�̳߳�Ҫ������������Դ
            if(!isPoolRunning_)
            {
                threads_.erase(threadid);
                std::cout<<"threadid:"<<std::this_thread::get_id()<<"exit"<<std::endl;
                exitCond_.notify_all();
                return;
            }
            if(poolMode_==PoolMode::MODE_CACHED)
            {
                //������������ʱ������
                //ÿһ�뷵��һ��
                //cachedģʽ�£��п����Ѿ������˺ܶ���̣߳����ǿ���ʱ�䳬��60s��Ӧ�ðѶ�����̻߳��յ�
                //���ճ���initThreadSize_�������߳�
                //��ǰʱ��-��һ��ִ�е�ʱ��
                //�߳�ͨ��
                if(std::cv_status::timeout==notEmpty_.wait_for(lock,std::chrono::seconds(1)))
                {
                    auto now=std::chrono::high_resolution_clock ().now();
                    auto dur=std::chrono::duration_cast<std::chrono::seconds>(now-lastTime);
                    if(dur.count()>=THREAD_MAX_IDLE_TIME&&curThreadSize_>initThreadSize_)
                    {
                        //��ʼ���յ�ǰ�߳�
                        //��¼�߳���������ص�ֵҪ�޸�
                        //���̶߳�����߳��б�������ɾ��   �����ѣ�
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

        idleThreadSize_--;  //�����߳�������һ
        //�߳�ȥ��������л�ȡ����
//        std::cout<<std::this_thread::get_id()<<"��������ɹ�"<<std::endl;
        auto task=taskQue_.front();
        taskQue_.pop();
        taskSize_--;
        lock.unlock();
        //ͨ��notFull�����߳�ͨ�ţ����Ѹ����¡�
        notFull_.notify_all();
        //����ִ��
//        task->run();
        task->exec();
        idleThreadSize_++;  //�����߳�������һ
        lastTime=std::chrono::high_resolution_clock().now();    //����
        std::cout<<std::this_thread::get_id()<<"����ִ�гɹ�"<<std::endl;
    }
}

//void ThreadPool::threadFunc(){  //���ﰴ����Ƶ��������
//
//    for(;;) {
//        std::shared_ptr<Task> ptr;
//        {
//            //��ȡ��
//            std::cout <<std::this_thread::get_id()<< "��ʼ������" << std::endl;
//            std::unique_lock<std::mutex> lock(taskQueMtx_);
//            //�߳�ͨ��
//            notEmpty_.wait(lock, [&] {
//                if (taskQue_.size() == 0)
//                    return false;
//                return true;
//            });
//            //�߳�ȥ��������л�ȡ����
////            std::cout <<std::this_thread::get_id()<< "��������ɹ�" << std::endl;
////        auto task=taskQue_.front();
//            ptr = taskQue_.front();
//            taskQue_.pop();
//            taskSize_--;
//            if(taskQue_.size()>0)
//                notEmpty_.notify_all();
//            //ͨ��notFull�����߳�ͨ�ţ����Ѹ����¡�
//            notFull_.notify_all();
//            //����ִ��
//        }
//        if(ptr!= nullptr){
//            ptr->run();
//            std::cout<<std::this_thread::get_id()<<"����ִ�гɹ�"<<std::endl;
//        }
//    }
//
//}

void ThreadPool::setMode(PoolMode mode) {   //����ģʽ
    if(checkRunningState())
    {
        return;
    }
    poolMode_=mode;
}

void ThreadPool::setTaskQueMaxThreshHold(int threshhold) {  //������������������ֵ
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

Result ThreadPool::submitTask(std::shared_ptr<Task> sp) { //�û��ύ����
    //��ȡ��
    std::unique_lock<std::mutex> lock(taskQueMtx_);
    //�߳�ͨ��
    //�û��ύ��������ȴ�ʱ�䳬����1s������ʾ�ύʧ�ܣ�����
    if(!notFull_.wait_for(lock,std::chrono::seconds(1),[&]{
            if(taskQue_.size()==taskQueMaxThreshHold_)
                return false;
             return true;
    })){
        std::cout<<"�����ύʧ��"<<std::endl;
        return Result(sp, false);
    }
    //������Ž����������
    taskQue_.emplace(sp);
    taskSize_++;
    //ͨ��notEmpty����֪ͨ
    notEmpty_.notify_all();
    //�����cachedģʽ������ȽϽ�����
    if(poolMode_==PoolMode::MODE_CACHED
    &&taskSize_>idleThreadSize_
    &&curThreadSize_<threadSizeThreshHold_)
    {
        //�������߳�
        auto p=std::make_unique<Thread>(std::bind(
                &ThreadPool::threadFunc,this,std::placeholders::_1));
        int threadId=p->getId();
        threads_.emplace(threadId,std::move(p));
        curThreadSize_++;
        //����
        threads_[threadId]->start();
        //�޸��߳���ر���
        curThreadSize_++;
        idleThreadSize_++;
    }
    return Result(sp);
}

Thread::Thread(Thread::ThreadFunc func)
:func_(func),threadId_(generateId_++)
{}

Thread::~Thread() {}

void Thread::start() {  //�߳��ഴ��һ���������е��߳�
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
    sem_.wait();    //task�������û��ִ���꣬����������û����߳�
    return std::move(any_);
}

void Result::setVal(Any any){
    this->any_=std::move(any);
    sem_.post();    //�Ѿ���ȡ����ķ���ֵ�������ź�����Դ
}

void Task::exec() {
    if(result_!= nullptr){
        result_->setVal(run()); //���﷢����̬
    }
}

void Task::setResult(Result *res) {
    result_=res;
}

Task::Task() :result_(nullptr){}
