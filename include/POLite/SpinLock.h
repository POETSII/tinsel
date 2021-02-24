#ifndef spin_lock_h
#define spin_lock_h

#include <atomic>
#include <cassert>
#include <thread>

/*
    We need a lightweight way of managing contention for access to
    devices. Spin locks are always a bit worrying, but as long as
    there is low contention they are not too bad.

    This could be more efficient with c++20 and test/notify/wait
*/
class SpinLock
{
private:
  std::atomic<int> _lock;
public:
  SpinLock()
  {
    _lock.store(0);
  }
  
  ~SpinLock()
  {
    assert(_lock.load()==0);
  }
  
  void lock()
  {
    // Bleh. hate spinlocks
    // This would be much more pleasant in c++20
    // This may kill performance...
    unsigned i=0;
    do{
       while(_lock.load(std::memory_order_relaxed)){
         __builtin_ia32_pause();
         if(i>100){
           std::this_thread::yield();
         }else{
           i++;
         }
       } 
    }while(_lock.exchange(1, std::memory_order_acquire));
  }

  void unlock()
  {
    _lock.store(0, std::memory_order_release);
  }
};

//using SpinLock = std::mutex;

template<bool Lock>
struct SpinLockGuard
{
private:
    SpinLock *m_lock;
public:
    SpinLockGuard()
        : m_lock(0)
    {}

    SpinLockGuard(SpinLock &lock)
        : m_lock(&lock)
    {
        lock.lock();
    }

    ~SpinLockGuard()
    {
        if(m_lock){
            m_lock->unlock();
        }
    }
};

template<>
struct SpinLockGuard<false>
{
public:
    SpinLockGuard()
    {}

    SpinLockGuard(SpinLock &lock)
    {}

    ~SpinLockGuard()
    {}
};


/*template<bool DoLock>
using SpinLockGuard = std::lock_guard<std::mutex>;*/

#endif
