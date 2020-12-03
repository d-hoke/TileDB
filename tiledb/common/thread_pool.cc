/**
 * @file   thread_pool.cc
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
 * This file defines the ThreadPool class.
 */

#include <cassert>

#include "tiledb/common/logger.h"
//#include "tiledb/sm/misc/thread_pool.h"
#include "thread_pool.h"

namespace tiledb {
namespace common {

// Define the static ThreadPool member variables.
std::unordered_map<std::thread::id, ThreadPool*> ThreadPool::tp_index_;
std::mutex ThreadPool::tp_index_lock_;

ThreadPool::ThreadPool()
    : concurrency_level_(0)
    , idle_threads_(0)
    , should_terminate_(false) {
}

ThreadPool::~ThreadPool() {
  terminate();
}

Status ThreadPool::init(const uint64_t concurrency_level) {
   //TBD: Nothing to prevent multiple initializations, do we care? (see if threads.size() > 0 ?)
  if (concurrency_level == 0) {
    return Status::ThreadPoolError(
        "Unable to initialize a thread pool with a concurrency level of 0.");
  }

  Status st = Status::Ok();

  // We allocate one less thread than `concurrency_level` because
  // the `wait_all*()` routines may service tasks concurrently with
  // the worker threads.
  const uint64_t num_threads = concurrency_level - 1;
  //TBD: is threads_ always going to be empty() on invocation, do we
  // want/need it to be?
  threads_.reserve(num_threads);
  for (uint64_t i = 0; i < num_threads; i++) {
    try {
      threads_.emplace_back([this]() { worker(*this); });
    } catch (const std::exception& e) {
      st = Status::ThreadPoolError(
          "Error initializing thread pool of concurrency level " +
          std::to_string(concurrency_level) + "; " + e.what());
      LOG_STATUS(st);
      break;
    }
  }

  if (!st.ok()) {
    // Join any created threads on error.
    terminate();
    return st;
  }

  // Save the concurrency level.
  concurrency_level_ = concurrency_level;

  // Index this ThreadPool instance from all of its thread ids.
  add_tp_index();

  return st;
}

ThreadPool::Task ThreadPool::execute(std::function<Status()>&& function) {
  if (concurrency_level_ == 0) {
    Task invalid_future;
    LOG_ERROR("Cannot execute task; thread pool uninitialized.");
    return invalid_future;
  }

  if (!function) {
    Task invalid_future;
    LOG_ERROR("Cannot execute task; invalid function.");
    return invalid_future;
  }

#if !NBQ
  std::unique_lock<std::mutex> ul(task_stack_mutex_);
#endif

  if (should_terminate_) {
    Task invalid_future;
    LOG_ERROR("Cannot execute task; thread pool has terminated.");
    return invalid_future;
  }

  PackagedTask task(std::move(function));
   if(task.refTaskState().use_count() != 1)
     {
	printf("pkgtask usecount != 1\n");
     asm
     (
	"int $3\n"
     );
     }
   
   
  ThreadPool::Task future = task.get_future();
   #if 0
   //The get_future() call of Task(TaskState &&state) does not actually result in swap, pkgdtask still owning share_ptr ->taskstate
   if(future.task_state_.use_count() !=1)
     {
	printf("future usecount != 1, is %ld\n",future.task_state_.use_count());
	printf("pkgtask usecount %ld\n", task.refTaskState().use_count());
     asm
     (
	"int $3\n"
     );
     }
   
      if( task.refTaskState().use_count() != 0 //actually expecting zero
	)
     {
	printf("pkgtask usecount non-zero, expected zero\n");
	asm
	  (
	     "int $3\n"
	  );
	
     }
   #endif
   

  // When we have a concurrency level > 1, we will have at least
  // one thread available to pick up the task. For a concurrency
  // level == 1, we have no worker threads available. When no
  // worker threads are available, execute the task on this
  // thread.
  if (concurrency_level_ == 1) {
#if !NBQ
    ul.unlock();
#endif
    task();
  } else {
    // Lookup the thread pool that this thread belongs to. If it
    // does not belong to a thread pool, `lookup_tp` will return
    // `nullptr`.
    ThreadPool* const tp = lookup_tp();

    // As both an optimization and a means of breaking deadlock,
    // execute the task if this thread belongs to `this`. Otherwise,
    // queue it for a worker thread.
    if (tp == this && idle_threads_.load() == 0) {
#if !NBQ
      ul.unlock();
#endif
      task();
    } else {
      // Add `task` to the stack of pending tasks.
#if !NBQ
      task_stack_.push(std::move(task));
      task_stack_cv_.notify_one();
#else
       task_queue_.enqueue(std::move(task));
#endif

      // The `ul` protects both `task_stack_` and `idle_threads_`,
      // save a copy of `idle_threads_` before releasing the lock.
      const uint64_t idle_threads_cp = idle_threads_.load();
#if !NBQ
      ul.unlock();
#endif

      // If all threads are busy, signal a thread in `this` that is
      // blocked waiting on another task. This wakes up one of those
      // threads to service the `task` that we just added to `task_stack_`.
      // There is a race here on `idle_threads_` because we just released
      // the lock. If a thread became idle and picks up `task`, we have
      // spuriously unlocked a thread in the `wait` path. It will find
      // that the `task_stack_` is empty and re-enter its wait.
      if (idle_threads_cp == 0) {
        blocked_tasks_mutex_.lock();
        if (!blocked_tasks_.empty()) {
          // Signal the first blocked task to wake up and check the task
          // stack for a task to execute.
          std::shared_ptr<TaskState> blocked_task = *blocked_tasks_.begin();
          {
            std::lock_guard<std::mutex> lg(blocked_task->return_st_mutex_);
            blocked_task->check_task_stack_ = true;
          }
          //blocked_task->cv_.notify_all();
          blocked_task->cv_.notify_one(); //tried, doesn't seem to help cpu utilization much...
	  //TBD: Could we skip erasure here, and then be able
	  //to not bother checking if present in wait_or_work()?
          blocked_tasks_.erase(blocked_task);
        }
        blocked_tasks_mutex_.unlock();
      }
    }
  }

  assert(future.valid());
  return future;
}

uint64_t ThreadPool::concurrency_level() const {
  return concurrency_level_;
}

Status ThreadPool::wait_all(std::vector<Task>& tasks) {
  auto statuses = wait_all_status(tasks);
  for (auto& st : statuses) {
    if (!st.ok()) {
      return st;
    }
  }
  return Status::Ok();
}

std::vector<Status> ThreadPool::wait_all_status(std::vector<Task>& tasks) {
  std::vector<Status> statuses;
  statuses.reserve(tasks.size());
  ThreadPool *mytp = lookup_tp();
  if(mytp == nullptr) mytp = this;
  for (auto& task : tasks) {
    if (!task.valid()) {
      LOG_ERROR("Waiting on invalid task future.");
      statuses.push_back(Status::ThreadPoolError("Invalid task future"));
    } else {
      Status status = wait_or_work(std::move(task), mytp);
      //Status status = wait_or_work(task);
      if (!status.ok()) {
        LOG_STATUS(status);
      }
      statuses.push_back(status);
    }
  }
  return statuses;
}

Status ThreadPool::wait_or_work(Task&& task, ThreadPool *p_mytp) {
  do {
    if (task.done())
      break;

    // Lookup the thread pool that this thread belongs to. If it
    // does not belong to a thread pool, `lookup_tp` will return
    // `nullptr`.
    if(!p_mytp) asm ("int $3\n");
    ThreadPool* tp = p_mytp ? p_mytp : lookup_tp();

    // If the calling thread exists outside of a thread pool, it may
    // service pending tasks from this thread pool.
    if (tp == nullptr) {
      tp = this;
    }

#if !NBQ
    // Lock the `tp->task_stack_` to receive the next task to work on.
    tp->task_stack_mutex_.lock();

    // If there are no pending tasks, we will wait for `task` to complete.
    if (tp->task_stack_.empty()) {
      // TBD: But, seems other tasks could be added after we just
      //checked and thus while we are waiting, in which case 
      //the attempted advantage is lost.  
      //Any solution? actually maybe not a problem, bcause...
      //OK, sposedly not a problem, the wait() below sposedly 
      //satisfied if new items come to task_stack_.
      tp->task_stack_mutex_.unlock();
#else
    PackagedTask inner_task;
    bool gottask ;//= false;
//c++14?    if (auto gotone = tp->task_queue_.try_dequeue(inner_task); !gottask=gotone) {
    if (! (gottask = tp->task_queue_.try_dequeue(inner_task))) {
#endif

      // Add `task` to `blocked_tasks_` so that the `execute()` path can
      // signal it when a new pending task is available.
      tp->blocked_tasks_mutex_.lock();
      //TBD: Seems this could be bad race between operator() executing
      //fn_() in worker thread hitting reset, and this thread which is
      //copying this shared pointer that is or may be reset...???
      tp->blocked_tasks_.insert(task.task_state_);
      tp->blocked_tasks_mutex_.unlock();

      // Block until the task is signaled. It will be signaled when it
      // has completed or when there is new work to execute on `task_stack_`.
      //TBD: ATM, appears task.wait() could be satisfied with spuriousness rather than
      //actually being signaled...
      task.wait();

      // Remove `task` from `blocked_tasks_`.
      tp->blocked_tasks_mutex_.lock();
      if (tp->blocked_tasks_.count(task.task_state_) > 0)
        tp->blocked_tasks_.erase(task.task_state_);
      tp->blocked_tasks_mutex_.unlock();

      // After the task has been woken up, check to see if it has completed.
      if (task.done()) {
        break;
      }

      // The task did not complete. This task has been signaled because a new
      // pending task was added to `task_stack_`. Reset the `check_task_stack_`
      // flag.
      {
        std::lock_guard<std::mutex> lg(task.task_state_->return_st_mutex_);
        task.task_state_->check_task_stack_ = false;
      }

#if !NBQ
      // Lock the `task_stack_` again before checking for the next pending task.
      tp->task_stack_mutex_.lock();
#endif
    }

    // We may have released and re-aquired the `task_stack_mutex_`. We must
    // check if it is still non-empty.
#if !NBQ
    if (!tp->task_stack_.empty()) {
      // Pull the next task off of the task stack. We specifically use a LIFO
      // ordering to prevent overflowing the call stack.
      //TBD: But, isn't LIFO going to starve threads at 'bottom' in
      //circumstances where keeping up isn't happening rather than
      //trying to keep things moving along, hoping 'recovery' will
      //occur and can 'catch up'?
      PackagedTask inner_task = std::move(tp->task_stack_.top());
      tp->task_stack_.pop();

      // We're done mutating `tp->task_stack_`.
      tp->task_stack_mutex_.unlock();
#else
    if (gottask) {
#endif

      // Execute the inner task.
      // TBD: should this be 'inner_task.valid()' rather than task.valid()?
      assert(inner_task.valid());
      inner_task();
    } else {
#if !NBQ
      // The task stack is now empty, retry.
      tp->task_stack_mutex_.unlock();
#endif
    }
  } while (true);

  // The task has completed and will not block.
  assert(task.done());
  return task.get();
}

void ThreadPool::terminate() {
  {
#if NBQ
    should_terminate_ = true;
#else
    std::unique_lock<std::mutex> ul(task_stack_mutex_);
    should_terminate_ = true;
//TBD: have to fix how worker threads get notified..
    task_stack_cv_.notify_all();
#endif
  }

  remove_tp_index();

  for (auto& t : threads_) {
    t.join();
  }

  threads_.clear();
}

void ThreadPool::worker(ThreadPool& pool) {
  while (true) {
    PackagedTask task;

    {
      // Wait until there's work to do.
#if !NBQ
       std::unique_lock<std::mutex> ul(pool.task_stack_mutex_);
#endif
      ++pool.idle_threads_;
#if NBQ
      if(!pool.task_queue_.wait_dequeue_timed(task,1000000)) {
	if (pool.should_terminate_) {
	  break;
	}
      }
#else
      if(pool.task_stack_.empty())
      {
	 
	 pool.task_stack_cv_.wait(ul, [&pool]() {
	    return pool.should_terminate_ || !pool.task_stack_.empty();
	 });
      }
       

      if (!pool.task_stack_.empty()) {
        task = std::move(pool.task_stack_.top());
        pool.task_stack_.pop();
        --pool.idle_threads_;
      } else {
        // The task stack was empty, ensure `task` is invalid.
	// TBD: How could task *ever* be valid at this point, should be freshly constructed...
	//don't seem to come here often if at all... asm("int $3\n");
        if (task.valid()) {
	   asm("int $3\n");
          task.reset();
          assert(!task.valid());
        }
      }
#endif
    }

    //TBD: If, just continue in else branch above, and move
    //pool terminate check to top of loop, this check for .valid()
    //should not be necessary...?
    if (task.valid())
      task();

    if (pool.should_terminate_)
      break;
  }
}

void ThreadPool::add_tp_index() {
  std::lock_guard<std::mutex> lock(tp_index_lock_);
  for (const auto& thread : threads_)
    tp_index_[thread.get_id()] = this;
}

void ThreadPool::remove_tp_index() {
  std::lock_guard<std::mutex> lock(tp_index_lock_);
  for (const auto& thread : threads_)
    tp_index_.erase(thread.get_id());
}

ThreadPool* ThreadPool::lookup_tp() {
  const std::thread::id tid = std::this_thread::get_id();
  std::lock_guard<std::mutex> lock(tp_index_lock_);
  if (tp_index_.count(tid) == 1)
    return tp_index_[tid];
  return nullptr;
}
   
//   CachingAllocator<std::shared_ptr<ThreadPool::TaskState>> ThreadPool::PackagedTask::task_state_cache_;

   //ThreadPool::task_state_pool_alloc ThreadPool::PackagedTask::tsalloc;
   //ThreadPool::PackagedTask::task_state_pool_alloc tsalloc;
   ThreadPool::PackagedTask::task_state_pool_alloc ThreadPool::PackagedTask::tsalloc;
   std::shared_ptr<ThreadPool::TaskState> ThreadPool::PackagedTask::allocprime_ = std::allocate_shared<ThreadPool::TaskState, decltype(tsalloc)>(tsalloc);

}  // namespace sm
}  // namespace tiledb
