#include "EventLoopThreadPool.h"

#include <assert.h>

#include "EventLoop.h"
#include "EventLoopThread.h"

EventLoopThreadPool::EventLoopThreadPool(EventLoop * baseLoop):
    _baseLoop(baseLoop),
    _next(0)
{
}

EventLoopThreadPool::~EventLoopThreadPool()
{
}

void EventLoopThreadPool::start(int numThreads)
{
    _baseLoop->assertInLoopThread();
    for(int i = 0; i < numThreads; ++i)
    {
        EventLoopThreadPtr elt(MakeEventLoopThreadPtr(i));
        _threads.emplace_back(elt);
        _loops.emplace_back(elt->startLoop());
    }
}

void EventLoopThreadPool::quit()
{
    _baseLoop->assertInLoopThread();
    for(size_t i = 0; i < _loops.size(); ++i)
    {
        _loops[i]->quit();
    }
}

EventLoop * EventLoopThreadPool::getNextLoop()
{
    _baseLoop->assertInLoopThread();
    EventLoop * loop = _baseLoop;
    if(!_loops.empty())
    {
        if(_next >= _loops.size())
        {
            _next = 0;
        }

        loop = _loops[_next++];
    }

    assert(loop != nullptr);
    return loop;
}

EventLoop * EventLoopThreadPool::getModLoop(int sessionId)
{
    _baseLoop->assertInLoopThread();
    EventLoop * loop = _baseLoop;
    if(!_loops.empty())
    {
        sessionId %= _loops.size();

        loop = _loops[sessionId];
    }

    return loop;
}
