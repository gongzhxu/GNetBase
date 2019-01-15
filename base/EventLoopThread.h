#ifndef _EVENT_LOOP_THREAD_
#define _EVENT_LOOP_THREAD_

#include <thread>
#include <mutex>
#include <condition_variable>

#include "EventLoop.h"

class EventLoopThread
{
public:
    EventLoopThread(int loopId = 0);
    ~EventLoopThread();
public:
    EventLoop * startLoop();
    EventLoop * getLoop();
    void stopLoop();
private:
    void threadFunc();

private:
    EventLoop loop_;

    std::thread thread_;
};

#endif
