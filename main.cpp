#include <iostream>
#include <thread>
#include <memory>
#include "threadpool.h"
/*
    example:   //ʹ��˵��
    ThreadPool pool;   //����һ���̳߳ض���
    pool.setMode(MODE_CACHED); //�����̳߳صĹ���ģʽ��Ĭ��Ϊfixedģʽ��
    pool.setTaskQueMaxThreshHold(1024)  //����������������������ֵ��Ĭ��Ϊ1024��
    pool.setThreadSizeThreshHold(1024)  //�����̵߳����������Ĭ��Ϊ10��
    pool.start(4); //�����̳߳صĳ�ʼ�߳�����������ʼ����
    class A:public Task{       //�Լ����������
        void run(){
            ���ܴ���~~;
        }
    }
    Result res=pool.submitTask(new A); //�ύ���񣬲����ؽ��
    std::cout<<res.get().cast_<int>()<<std::endl;   //������
*/


class A:public Task{       //�Լ����������
    Any run(){
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return 0;
    }
};

int main()
{
    ThreadPool pool;
    pool.setTaskQueMaxThreshHold(4);
    pool.start(4);  //��ʼ����
    Result res1 = pool.submitTask(std::make_shared<A>());
    std::cout<<res1.get().cast_<int>()<<std::endl;
    return 0;
}
