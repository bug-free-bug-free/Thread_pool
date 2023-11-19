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

class Task{ //����������
public:
    Task();
    ~Task()=default;
    virtual Any run()=0;
    void exec();
    void setResult(Result* res);
private:
    Result* result_;
};

class Any{  //ʵ��Any��
public:
    Any()=default;
    ~Any()=default;
    Any(const Any&)=delete;
    Any& operator=(const Any&)=delete;
    Any(Any&&)=default;
    Any& operator=(Any&&)=default;

    //������캯��������Any������������͵�����
    template<class T>
    Any(T data): base_(std::make_unique<Derive<T>>(data)){}

    template<class T>
    T cast_()
    {
        //������ô��base_�ҵ�����ָ���Derive���󣬴ӵ���ȥ��ȡ��data��Ա����
        //ʹ��RTTI
        Derive<T>* pd=dynamic_cast<Derive<T>*>(base_.get());
        if(pd== nullptr){
            throw "type is unmatch!";
        }
        return pd->data_;
    }
private:
    //��������
    class Base
    {
    public:
        virtual ~Base()=default;
    };
    //����������
    template<class T>
    class Derive: public Base
    {
    public:
        Derive(T data): data_(data)
        {}
        T data_;
    };
private:
    //������һ�������ָ��
    std::unique_ptr<Base> base_;
};

class Semaphore{    //�ź�������
public:
    Semaphore(int limit=0):resLimit_(limit){}
    ~Semaphore()=default;
    void wait(){    //��ȡ�ź���
        std::unique_lock<std::mutex> lock(mtx_);
        cond_.wait(lock,[&]{
            return resLimit_>0;
        });
        resLimit_--;
    }
    void post(){    //�ͷ��ź���
        std::unique_lock<std::mutex> lock(mtx_);
        resLimit_++;
        cond_.notify_all();
    }
private:
    int resLimit_;
    std::mutex mtx_;
    std::condition_variable cond_;
};

//ʵ�ֽ�����ɽ���task����ķ���ֵ����Result
class Result{
public:
    Result()=default;
    Result(std::shared_ptr<Task> task,bool isValid=true);
    ~Result()=default;
    Any get();//�û�����get������ȡ�û��ķ���ֵ
    void setVal(Any any);
private:
    Any any_;   //�洢�����Any��
    Semaphore sem_; //�߳�ͨ���ź���
    std::shared_ptr<Task> task_; //ֻ���Ӧ��ȡ������������
    std::atomic_bool isValid_;   //����ֵ�Ƿ���Ч
};

enum PoolMode{  //�̳߳�֧�ֵ�ģʽ
    MODE_FIXED,
    MODE_CACHED
};

class Thread {  //�߳�����
public:
    using ThreadFunc=std::function<void(int)>; //�̺߳�����������
    Thread(ThreadFunc func);
    ~Thread();
    void start();
    int getId()const;
private:
    ThreadFunc func_;
    static int generateId_;
    int threadId_;  //�����߳�id
};

class ThreadPool {  //�̳߳���
public:
    ThreadPool();
    ~ThreadPool();

    bool checkRunningState()const;
    void start(int size=std::thread::hardware_concurrency());   //�����̳߳�
    void setMode(PoolMode mode);    //�����̳߳صĹ���ģʽ
    void setTaskQueMaxThreshHold(int threshhold);   //������������������ֵ
    void setThreadSizeThreshHold(int threshhold);   //�����̵߳��������
    Result submitTask(std::shared_ptr<Task> sp);    //���̳߳��ύ����

    ThreadPool(const ThreadPool& t)=delete;
    void operator=(const ThreadPool& t)=delete;
    void threadFunc(int threadid);
private:
//    std::vector<std::unique_ptr<Thread>> threads_;//�߳��б�
    std::unordered_map<int,std::unique_ptr<Thread>> threads_;   //�߳��б�
    int  initThreadSize_;       //��ʼ���߳�����
    int threadSizeThreshHold_;  //�߳���������
    std::atomic_int curThreadSize_; //��¼��ǰ�̳߳����̵߳�������
    std::atomic_int idleThreadSize_;    //��¼�����̵߳�����

    std::queue<std::shared_ptr<Task>> taskQue_;     //�������
    std::atomic_int taskSize_;      //��������
    int taskQueMaxThreshHold_;      //��������������ֵ

    std::mutex taskQueMtx_;     //��֤��������̰߳�ȫ
    std::condition_variable notFull_;       //������в���
    std::condition_variable exitCond_;  //�ȵ��߳���Դȫ������
    std::condition_variable notEmpty_;      //������в���
    PoolMode poolMode_;      //��ǰ�̳߳صĹ���ģʽ
    std::atomic_bool isPoolRunning_  ;//��ʾ��ǰ�̳߳ص�����״̬

};

#endif //_THREADPOOL_H