/**
 * @file   thread_pool.h
 *
 * @section LICENSE
 *
 * The MIT License
 *
 * @copyright Copyright (c) 2018-2020 TileDB, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * @section DESCRIPTION
 *
 * This file declares the ThreadPool class.
 */

#ifndef TILEDB_THREAD_POOL_H
#define TILEDB_THREAD_POOL_H

#include <condition_variable>
#include <mutex>
#include <stack>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

//#include "tiledb_export.h"
#include "tiledb.h" //to get tiledb_export.h for examples as well as mainline

#include <boost/pool/pool_alloc.hpp>

//#include <moodyCamel/blockingconcurrentqueue.h>
#include "moodyCamel/blockingconcurrentqueue.h"
#define NBQ 0
/** when !NBQ
 * itrs 0 of 1000
 * good 1000000, bad 0
 * 
 * real    0m9.721s
 * user    0m10.774s
 * sys     0m15.344s
itrs 0 of 20000
 * itrs 1000 of 20000
 * itrs 2000 of 20000
 * itrs 3000 of 20000
 * itrs 4000 of 20000
 * itrs 5000 of 20000
 * itrs 6000 of 20000
 * itrs 7000 of 20000
 * itrs 8000 of 20000
 * itrs 9000 of 20000
 * itrs 10000 of 20000
 * itrs 11000 of 20000
 * itrs 12000 of 20000
 * itrs 13000 of 20000
 * itrs 14000 of 20000
 * itrs 15000 of 20000
 * itrs 16000 of 20000
 * itrs 17000 of 20000
 * itrs 18000 of 20000
 * itrs 19000 of 20000
 * good 20000000, bad 0
 * 
 * real    3m16.879s
 * user    3m41.987s
 * sys     5m0.648s
 * 
itrs 0 of 20000
 * itrs 1000 of 20000
 * itrs 2000 of 20000
 * itrs 3000 of 20000
 * itrs 4000 of 20000
 * itrs 5000 of 20000
 * itrs 6000 of 20000
 * itrs 7000 of 20000
 * itrs 8000 of 20000
 * itrs 9000 of 20000
 * itrs 10000 of 20000
 * itrs 11000 of 20000
 * itrs 12000 of 20000
 * itrs 13000 of 20000
 * itrs 14000 of 20000
 * itrs 15000 of 20000
 * itrs 16000 of 20000
 * itrs 17000 of 20000
 * xitrs 18000 of 20000
 * itrs 19000 of 20000
 * good 20000000, bad 0
 * 
 * real    3m20.386s
 * user    3m40.143s
 * sys     4m54.638s
 **/
/** when NBQ
itrs 0 of 1000
 * good 1000000, bad 0
 * 
 * real    0m3.715s
 * user    0m6.795s
 * sys     0m0.117s
itrs 0 of 20000
 itrs 1000 of 20000
 itrs 2000 of 20000
 itrs 3000 of 20000
 itrs 4000 of 20000
 itrs 5000 of 20000
 itrs 6000 of 20000
 itrs 7000 of 20000
 itrs 8000 of 20000
 itrs 9000 of 20000
 itrs 10000 of 20000
 itrs 11000 of 20000
 itrs 12000 of 20000
 itrs 13000 of 20000
 itrs 14000 of 20000
 itrs 15000 of 20000
 itrs 16000 of 20000
 itrs 17000 of 20000
 itrs 18000 of 20000
 itrs 19000 of 20000
 good 20000000, bad 0
 
 real    0m53.999s
 user    2m13.475s
 sys     0m2.210s
itrs 0 of 20000
*
 * itrs 1000 of 20000
 * itrs 2000 of 20000
 * itrs 3000 of 20000
 * itrs 4000 of 20000
 * itrs 5000 of 20000
 * itrs 6000 of 20000
 * itrs 7000 of 20000
 * itrs 8000 of 20000
 * itrs 9000 of 20000
 * itrs 10000 of 20000
 * itrs 11000 of 20000
 * itrs 12000 of 20000
 * itrs 13000 of 20000
 * itrs 14000 of 20000
 * itrs 15000 of 20000
 * itrs 16000 of 20000
 * itrs 17000 of 20000
 * itrs 18000 of 20000
 * itrs 19000 of 20000
 * good 20000000, bad 0
 * 
 * real    0m55.153s
 * user    2m15.228s
 * sys     0m2.278s
 ***/

#include "tiledb/common/logger.h"
#include "tiledb/common/status.h"
#include "tiledb/sm/misc/macros.h"

#include "cachingallocator.h"

namespace tiledb {
namespace common {

/**
 * A recusive-safe thread pool.
 */
class TILEDB_EXPORT ThreadPool {
 private:
  /* ********************************* */
  /*          PRIVATE DATATYPES        */
  /* ********************************* */

  // Forward-declaration.
  struct TaskState;

  // Forward-declaration.
  class PackagedTask;

 public:
  /* ********************************* */
  /*          PUBLIC DATATYPES         */
  /* ********************************* */

  class Task {
   public:
    /** Constructor. */
    Task()
        : task_state_(nullptr) {
    }

    /** Move constructor. */
    Task(Task&& rhs) {
      task_state_ = std::move(rhs.task_state_);
    }

    /** Move-assign operator. */
    Task& operator=(Task&& rhs) {
      task_state_ = std::move(rhs.task_state_);
      return *this;
    }

    /** Returns true if this instance is associated with a valid task. */
    bool valid() {
      return task_state_ != nullptr;
    }

   private:
    /** Value constructor. */
    Task(const std::shared_ptr<TaskState>& task_state)
       //TBD: 
       //first note, This constructor seems only used by PackagedTask's
       //get_future()...
       //But, if we're copying the shared_ptr with its current state
       //(of ref counts)
       //into that Task returned by get_future(), aren't we then
       //going to be dtor'ing the shared_ptr with an invalid state, as
       //a copy of it with same values is going to be in more than one
       //entity...???
        //: task_state_(std::move(task_state)) { //likely using copy constructor since formal param 'const'... and use_count() not zeroed when get_future() of pkgdtask called...
        : task_state_(task_state) {
	   //static_assert(std::is_rvalue_reference<std::move(task_state)>::value, "task_state not rvalueref even with std::move()?");
//	   static_assert(std::is_rvalue_reference<decltype(std::move(task_state))>::value, "task_state not rvalueref even with std::move()?");
//	   static_assert(!std::is_rvalue_reference<decltype(std::move(task_state))>::value, "task_state is rvalueref with std::move()?");
    }

    DISABLE_COPY_AND_COPY_ASSIGN(Task);

   /** Blocks until the task has completed or there are other tasks to service.
     */
    void wait() {
      std::unique_lock<std::mutex> ul(task_state_->return_st_mutex_);
      if (!task_state_->return_st_set_ && !task_state_->check_task_stack_)
      {
	//TBD: Isn't this subject to spurious wakeup, and if so, does
	//it actually cause any clients a problem?
        task_state_->cv_.wait(ul);
	//checking to see if spurious wakeup happens
	if (!task_state_->return_st_set_ && !task_state_->check_task_stack_)
	{
	   asm ( "int $3\n");
	}
      }
       
    }

    /** Returns true if the associated task has completed. */
    bool done() {
      std::lock_guard<std::mutex> lg(task_state_->return_st_mutex_);
      return task_state_->return_st_set_;
    }

    /**
     * Returns the result value from the task. If the task
     * has not completed, it will wait.
     */
    Status get() {
      wait(); //TBD: subject to spurious wakeup, may *not* be completed???
      //TBD: room for race condition here between wait() and following
      //lock, anybody capable of encountering it?
      std::lock_guard<std::mutex> lg(task_state_->return_st_mutex_);
      //TBD: would this occasionally 'assert(task_state_->return_st_set_);' ?
      return task_state_->return_st_;
    }

    /** The shared task state between futures and their associated task. */
    std::shared_ptr<TaskState> task_state_;

    friend ThreadPool;
    friend PackagedTask;
  };

  /* ********************************* */
  /*     CONSTRUCTORS & DESTRUCTORS    */
  /* ********************************* */

  /** Constructor. */
  ThreadPool();

  /** Destructor. */
  ~ThreadPool();

  /* ********************************* */
  /*                API                */
  /* ********************************* */

  /**
   * Initialize the thread pool.
   *
   * @param concurrency_level Maximum level of concurrency.
   * @return Status
   */
  Status init(uint64_t concurrency_level = 1);

  /**
   * Schedule a Task to execute a function. If the returned `Task` object
   * is valid, `function` is guaranteed to execute. The 'function' may
   * execute immediately on the calling thread. To avoid deadlock, `function`
   * should not aquire non-recursive locks held by the calling thread.
   *
   * @param function Task function to execute.
   * @return Task for the return status of the task.
   */
  Task execute(std::function<Status()>&& function);

  /** Return the maximum level of concurrency. */
  uint64_t concurrency_level() const;

  /**
   * Wait on all the given tasks to complete. This is safe to call recusively
   * and may execute pending tasks on the calling thread while waiting.
   *
   * @param tasks Task list to wait on.
   * @return Status::Ok if all tasks returned Status::Ok, otherwise the first
   * error status is returned
   */
  Status wait_all(std::vector<Task>& tasks);

  /**
   * Wait on all the given tasks to complete, return a vector of their return
   * Status. This is safe to call recusively and may execute pending tasks
   * on the calling thread while waiting.
   *
   * @param tasks Task list to wait on
   * @return Vector of each task's Status.
   */
  std::vector<Status> wait_all_status(std::vector<Task>& tasks);

//  using task_state_pool_alloc = boost::pool_allocator<TaskState>;
//  static task_state_pool_alloc tsalloc;
//  const std::shared_ptr<TaskState> allocprime_ = std::allocate_shared<TaskState, task_state_pool_alloc>(tsalloc);

 private:
  /* ********************************* */
  /*          PRIVATE DATATYPES        */
  /* ********************************* */

  struct TaskState {
    /** Constructor. */
    TaskState()
        : return_st_()
        , check_task_stack_(false)
        , return_st_set_(false) {
    }

    DISABLE_COPY_AND_COPY_ASSIGN(TaskState);
    DISABLE_MOVE_AND_MOVE_ASSIGN(TaskState);

    /** The return status from an executed task. */
    Status return_st_;

    bool check_task_stack_;

    /** True if the `return_st_` has been set. */
    bool return_st_set_;

    /** Waits for a task to complete. */
    std::condition_variable cv_;

    /** Protects `return_st_`, `return_st_set_`, and `cv_`. */
    std::mutex return_st_mutex_;
  };

  //Creating a pool_allocator object doesn't actually do an allocation
  //acts as a reference to a set of singleton allocators
  //boost::pool_allocator<TaskState> alloc;
  //This call causes a pool_allocator to be crated that gets 1040 bytes
  //from malloc - enough for 32 shared_ptrs to integers including the
  //integer.
//  using task_state_pool_alloc = boost::pool_allocator<TaskState>;
//  static task_state_pool_alloc tsalloc;
//  const std::shared_ptr<TaskState> allocprime_ = std::allocate_shared<TaskState, task_state_pool_alloc>(tsalloc);

  class PackagedTask {
   friend moodycamel::BlockingConcurrentQueue<PackagedTask>;
   friend moodycamel::ConcurrentQueue<PackagedTask>;
//   static CachingAllocator<std::shared_ptr<TaskState>> task_state_cache_;
//   static CachingAllocator<std::shared_ptr<TaskState>> task_state_cache_;
   using task_state_pool_alloc = boost::pool_allocator<TaskState>;
   static ThreadPool::PackagedTask::task_state_pool_alloc tsalloc;
   static std::shared_ptr<ThreadPool::TaskState> allocprime_ ;//= std::allocate_shared<TaskState, task_state_pool_alloc>(tsalloc);
     
   public:
    /** Constructor. */
    PackagedTask()
        : fn_(nullptr)
        , task_state_(nullptr) {
    }

    /** Value constructor. */
    template <class Fn_T>
    explicit PackagedTask(Fn_T&& fn) {
      fn_ = std::move(fn);
      //task_state_ = std::make_shared<TaskState>();
      task_state_ = std::allocate_shared<TaskState,task_state_pool_alloc>(tsalloc);
      //std::allocator_traits<cachingallocator<std::shared_ptr<TaskState>>>();
      //task_state_ = std::allocate_shared<TaskState>(task_state_cache_,0);
    }

    /** Move constructor. */
    PackagedTask(PackagedTask&& rhs) {
      fn_ = std::move(rhs.fn_);
      task_state_ = std::move(rhs.task_state_);
    }

    /** Move-assign operator. */
    PackagedTask& operator=(PackagedTask&& rhs) {
      fn_ = std::move(rhs.fn_);
      task_state_ = std::move(rhs.task_state_);
      return *this;
    }

    void reset() {
      fn_ = std::function<Status()>();
      task_state_ = nullptr;
    }

    /** Function-call operator. */
    void operator()() {
      const Status r = fn_();
      {
        std::lock_guard<std::mutex> lg(task_state_->return_st_mutex_);
        task_state_->return_st_set_ = true;
        task_state_->return_st_ = r;
      }
      //TBD: any circumstances where anyone could modify 
      // return_st_* before notified and trying to access it?
      // (cuz it gets zapped by reset() just below)
      task_state_->cv_.notify_all();

      //TBD: will all notified parties see return_st_* before
      //reset() zaps task_state_?
      reset();
    }

    /** Returns the future associated with this task. */
    ThreadPool::Task get_future() {
      return Task(task_state_);
    }

    /** Returns true if this instance has a valid task. */
    bool valid() const {
      return fn_ && task_state_ != nullptr;
    }

     const std::shared_ptr<TaskState> &refTaskState() const
       {
	  return task_state_;
       }
     
   private:
    DISABLE_COPY_AND_COPY_ASSIGN(PackagedTask);

    /** The packaged function. */
    std::function<Status()> fn_;

    /** The task state to share with futures. */
    std::shared_ptr<TaskState> task_state_;
  };

  /* ********************************* */
  /*         PRIVATE ATTRIBUTES        */
  /* ********************************* */

  /**
   * The maximum level of concurrency among a single waiter and all
   * of the the `threads_`.
   */
  uint64_t concurrency_level_;

#if defined(NBQ) && !NBQ
  /** Protects `task_stack_` and `idle_threads_`. */
  std::mutex task_stack_mutex_;

  /** Notifies work threads to check `task_stack_` for work. */
  std::condition_variable task_stack_cv_;
  /** Pending tasks in LIFO ordering. */
   std::stack<PackagedTask> task_stack_;
   //std::stack<PackagedTask,std::vector<PackagedTask>> task_stack_;
#else

   /** giving up LIFO, not sure why it was used? **/
   moodycamel::BlockingConcurrentQueue<PackagedTask> task_queue_;

#endif

  /**
   * The number of threads waiting for the `task_stack_` to
   * become non-empty.
   */
   std::atomic<uint64_t> idle_threads_;

  /** The worker threads. */
  std::vector<std::thread> threads_;

  /** When true, all pending tasks will remain unscheduled. */
  bool should_terminate_;

  /** All tasks that threads in this instance are waiting on. */
  struct BlockedTasksHasher {
    size_t operator()(const std::shared_ptr<TaskState>& task) const {
      return reinterpret_cast<size_t>(task.get());
    }
  };

  std::unordered_set<std::shared_ptr<TaskState>, BlockedTasksHasher>
      blocked_tasks_;

  /** Protects `blocked_tasks_`. */
  std::mutex blocked_tasks_mutex_;

  /** Indexes thread ids to the ThreadPool instance they belong to. */
  static std::unordered_map<std::thread::id, ThreadPool*> tp_index_;

  /** Protects 'tp_index_'. */
  static std::mutex tp_index_lock_;

  /* ********************************* */
  /*          PRIVATE METHODS          */
  /* ********************************* */

  /**
   * Waits for `task`, but will execute other tasks from `task_stack_`
   * while waiting. While this may be an performance optimization
   * to perform work on this thread rather than waiting, the primary
   * motiviation is to prevent deadlock when tasks are enqueued recursively.
   */
  Status wait_or_work(Task&& task, ThreadPool *p_mytp=nullptr);

  /** Terminate the threads in the thread pool. */
  void terminate();

  /** The worker thread routine. */
  static void worker(ThreadPool& pool);

  // Add indexes from each thread to this instance.
  void add_tp_index();

  // Remove indexes from each thread to this instance.
  void remove_tp_index();

  // Lookup the thread pool instance from the calling thread.
  ThreadPool* lookup_tp();
};

}  // namespace sm
}  // namespace tiledb

#endif  // TILEDB_THREAD_POOL_H
