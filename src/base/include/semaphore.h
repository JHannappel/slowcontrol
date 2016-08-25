#include <mutex>
#include <condition_variable>

class semaphore
{
public:

  semaphore(int count_ = 0) : count{count_}
  {}

  void notify()
  {
    std::unique_lock<mutex> lck(mtx);
    ++count;
    cv.notify_one();
  }

  void wait()
  {
    std::unique_lock<mutex> lck(mtx);
    while(count == 0)
    {
      cv.wait(lck);
    }

    --count;
  }

private:

  std::mutex mtx;
  std::condition_variable cv;
  int count;
};
