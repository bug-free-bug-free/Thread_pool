#include <iostream>
#include <thread>
#include <memory>
#include "threadpool.h"
/*
    example:   //使用说明
    ThreadPool pool;   //创建一个线程池对象
    pool.setMode(MODE_CACHED); //设置线程池的工作模式（默认为fixed模式）
    pool.setTaskQueMaxThreshHold(1024)  //设置设置任务队列数量最大值（默认为1024）
    pool.setThreadSizeThreshHold(1024)  //设置线程的最大数量（默认为10）
    pool.start(4); //设置线程池的初始线程数量，并开始运行
    class A:public Task{       //自己定义的子类
        void run(){
            功能代码~~;
        }
    }
    Result res=pool.submitTask(new A); //提交任务，并返回结果
    std::cout<<res.get().cast_<int>()<<std::endl;   //输出结果
*/


class A:public Task{       //自己定义的子类
    Any run(){
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return 0;
    }
};

int main()
{
    ThreadPool pool;
    pool.setTaskQueMaxThreshHold(4);
    pool.start(4);  //开始启动
    Result res1 = pool.submitTask(std::make_shared<A>());
    std::cout<<res1.get().cast_<int>()<<std::endl;
    return 0;
}
