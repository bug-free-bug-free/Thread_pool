#ifndef _THREADPOOL_H
#define _THREADPOOL_H
#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unordered_map>
class Any;
class Semaphore;
class Result;

class Task{ //任务抽象基类
public:
    Task();
    ~Task()=default;
    virtual Any run()=0;
    void exec();
    void setResult(Result* res);
private:
    Result* result_;
};

class Any{  //实现Any类
public:
    Any()=default;
    ~Any()=default;
    Any(const Any&)=delete;
    Any& operator=(const Any&)=delete;
    Any(Any&&)=default;
    Any& operator=(Any&&)=default;

    //这个构造函数可以让Any类接收任意类型的数据
    template<class T>
    Any(T data): base_(std::make_unique<Derive<T>>(data)){}

    template<class T>
    T cast_()
    {
        //我们怎么从base_找到他所指向的Derive对象，从当中去除取出data成员变量
        //使用RTTI
        Derive<T>* pd=dynamic_cast<Derive<T>*>(base_.get());
        if(pd== nullptr){
            throw "type is unmatch!";
        }
        return pd->data_;
    }
private:
    //基类类型
    class Base
    {
    public:
        virtual ~Base()=default;
    };
    //派生类类型
    template<class T>
    class Derive: public Base
    {
    public:
        Derive(T data): data_(data)
        {}
        T data_;
    };
private:
    //定义了一个基类的指针
    std::unique_ptr<Base> base_;
};

class Semaphore{    //信号量机制
public:
    Semaphore(int limit=0):resLimit_(limit){}
    ~Semaphore()=default;
    void wait(){    //获取信号量
        std::unique_lock<std::mutex> lock(mtx_);
        cond_.wait(lock,[&]{
            return resLimit_>0;
        });
        resLimit_--;
    }
    void post(){    //释放信号量
        std::unique_lock<std::mutex> lock(mtx_);
        resLimit_++;
        cond_.notify_all();
    }
private:
    int resLimit_;
    std::mutex mtx_;
    std::condition_variable cond_;
};

//实现接收完成接受task任务的返回值类型Result
class Result{
public:
    Result()=default;
    Result(std::shared_ptr<Task> task,bool isValid=true);
    ~Result()=default;
    Any get();//用户调用get方法获取用户的返回值
    void setVal(Any any);
private:
    Any any_;   //存储任务的Any类
    Semaphore sem_; //线程通信信号量
    std::shared_ptr<Task> task_; //只想对应获取人物的任务对象
    std::atomic_bool isValid_;   //返回值是否有效
};

enum PoolMode{  //线程池支持的模式
    MODE_FIXED,
    MODE_CACHED
};

class Thread {  //线程类型
public:
    using ThreadFunc=std::function<void(int)>; //线程函数对象类型
    Thread(ThreadFunc func);
    ~Thread();
    void start();
    int getId()const;
private:
    ThreadFunc func_;
    static int generateId_;
    int threadId_;  //保存线程id
};

class ThreadPool {  //线程池类
public:
    ThreadPool();
    ~ThreadPool();

    bool checkRunningState()const;
    void start(int size=std::thread::hardware_concurrency());   //开启线程池
    void setMode(PoolMode mode);    //设置线程池的工作模式
    void setTaskQueMaxThreshHold(int threshhold);   //设置任务队列数量最大值
    void setThreadSizeThreshHold(int threshhold);   //设置线程的最大数量
    Result submitTask(std::shared_ptr<Task> sp);    //给线程池提交任务

    ThreadPool(const ThreadPool& t)=delete;
    void operator=(const ThreadPool& t)=delete;
    void threadFunc(int threadid);
private:
//    std::vector<std::unique_ptr<Thread>> threads_;//线程列表
    std::unordered_map<int,std::unique_ptr<Thread>> threads_;   //线程列表
    int  initThreadSize_;       //初始的线程数量
    int threadSizeThreshHold_;  //线程数量上限
    std::atomic_int curThreadSize_; //记录当前线程池中线程的总数量
    std::atomic_int idleThreadSize_;    //记录空闲线程的数量

    std::queue<std::shared_ptr<Task>> taskQue_;     //任务队列
    std::atomic_int taskSize_;      //任务数量
    int taskQueMaxThreshHold_;      //任务队列数量最大值

    std::mutex taskQueMtx_;     //保证任务队列线程安全
    std::condition_variable notFull_;       //任务队列不满
    std::condition_variable exitCond_;  //等到线程资源全部回收
    std::condition_variable notEmpty_;      //任务队列不空
    PoolMode poolMode_;      //当前线程池的工作模式
    std::atomic_bool isPoolRunning_  ;//表示当前线程池的启动状态

};

#endif //_THREADPOOL_H