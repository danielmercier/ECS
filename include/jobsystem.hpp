#pragma once

#include <thread>
#include <atomic>
#include <functional>
#include <algorithm>
#include <optional>

#include "concurrentqueue.h"
#include "blockingconcurrentqueue.h"

using Version = size_t;

using JobId = size_t;

struct JobHandle
{
   JobId id;
   Version version;
};

class JobPool
{
public:
   JobPool() : m_pool(new std::array<Job, POOL_SIZE>()), m_version(new std::array<std::atomic<size_t>, POOL_SIZE>())
   {
      std::vector<size_t> available(POOL_SIZE);

      for (size_t i = 0; i < POOL_SIZE; ++i)
      {
         available[i] = i;
      }

      // Fill the queue with the current available ids, which are all ids
      m_available.enqueue_bulk(available.begin(), POOL_SIZE);
   };

   // Thread safe
   bool create(std::function<void()>&& task, JobHandle& handle)
   {
      size_t next_free;

      if (!m_available.try_dequeue(next_free))
      {
         return false;
      }

      handle.id = next_free;
      handle.version = (*m_version)[next_free].load(std::memory_order_relaxed);

      (*m_pool)[next_free].init(std::move(task));

      return true;
   }

   // Thread safe
   bool create(std::function<void()>&& task, JobHandle& handle, JobHandle parent)
   {
      size_t next_free;

      if (!m_available.try_dequeue(next_free))
      {
         return false;
      }

      handle.id = next_free;
      handle.version = (*m_version)[next_free].load(std::memory_order_relaxed);

      (*m_pool)[next_free].init(std::move(task), parent);

      // This is not very safe, the user is resonsible of scheduling the parent after children
      (*m_pool)[parent.id].m_unfinished.fetch_add(1, std::memory_order_relaxed);

      return true;
   }

   void addContinuation(JobHandle parent, JobHandle continuation)
   {
      (*m_pool)[parent.id].m_continuations.push_back(continuation);
   }

   // Thread safe for a given handle
   // Should not be called from 2 threads with the same handle.id
   // Return the jobs that can be scheduled
   std::vector<JobHandle> invoke(JobHandle handle)
   {
      std::vector<JobHandle> continuations;

      // First run the task associated to this handle
      (*m_pool)[handle.id].m_task();

      finish(handle, continuations);

      return continuations;
   }

   void finish(JobHandle handle, std::vector<JobHandle>& continuations)
   {
      Job& job = (*m_pool)[handle.id];
      job.m_unfinished.fetch_sub(1, std::memory_order_release);

      if (job.m_parent.has_value())
      {
         finish(job.m_parent.value(), continuations);
      }

      if (job.m_unfinished.load(std::memory_order_acquire) <= 0)
      {
         // Invalidate this job by incrementing the version in the m_version
         // This also indicates that the job is finished
         (*m_version)[handle.id].fetch_add(1, std::memory_order_release);

         // We call this now, because by adding 1 to the version, we are sure that finished returns true
         // this means that it is safe to get the registration now. But BEFORE adding handle.id to the queue
         // to not take the continuation of another handle.
         std::vector<JobHandle> newContinuations = job.m_continuations;
         continuations.insert(continuations.end(), newContinuations.begin(), newContinuations.end());

         // And add the fact that this id is now available to use by another job
         m_available.enqueue(handle.id);
      }
   }

   // Thread safe
   bool finished(JobHandle handle)
   {
      // A job is finished when it has been released calling invoke
      return handle.version < (*m_version)[handle.id].load(std::memory_order_acquire);
   }

private:
   struct Job {
      std::function<void()> m_task;
      
      // Parent of this job
      // The parent is finished when all its children are finished
      std::optional<JobHandle> m_parent;
      std::atomic<size_t> m_unfinished;

      // Jobs that should be executed when this job is finised
      std::vector<JobHandle> m_continuations;

      void init(std::function<void()>&& task)
      {
         m_task = std::move(task);
         m_parent = std::nullopt;
         m_unfinished.store(1, std::memory_order_relaxed);
         m_continuations.clear();
      }

      void init(std::function<void()>&& task, JobHandle parent)
      {
         init(std::move(task));
         m_parent = parent;
      }
   };

   static constexpr size_t POOL_SIZE = 65536;
   std::unique_ptr<std::array<Job, POOL_SIZE>> m_pool;
   std::unique_ptr<std::array<std::atomic<size_t>, POOL_SIZE>> m_version;
   moodycamel::ConcurrentQueue<size_t> m_available;

};

class JobSystem
{
public:
   JobSystem() : m_pending(0)
   {
      const size_t thread_count = std::max(static_cast<unsigned int>(1), std::thread::hardware_concurrency() - 1);
      m_wokers.reserve(thread_count);

      for (size_t i = 0; i < thread_count; i++)
      {
         m_wokers.emplace_back([this] {
            JobHandle job;

            while (true)
            {
               m_ready_queue.wait_dequeue(job);
               work_one(job);
            }
         });
         m_wokers[i].detach();
      }
   }

   // Create a task (do not schedule it)
   JobHandle create(std::function<void()>&& task)
   {
      JobHandle handle;
      while (!m_job_pool.create(std::move(task), handle))
      {
         // Work until the job pool can get a new job
         try_work();
      }

      return handle;
   }

   // Create a new task with a given parent.
   // The parent is not a dependency, it is meant to be used if you want to wait on multiple jobs
   // wait(parent) will wait that all childs are finished
   JobHandle create(std::function<void()>&& task, JobHandle parent)
   {
      JobHandle handle;
      while (!m_job_pool.create(std::move(task), handle, parent))
      {
         // Work until the job pool can get a new job
         try_work();
      }

      return handle;
   }

   void schedule(JobHandle handle)
   {
      m_ready_queue.enqueue(handle);
      m_pending.fetch_add(1, std::memory_order_release);
   }

   void schedule(JobHandle handle, JobHandle dependency)
   {
      if (m_job_pool.finished(dependency))
      {
         m_ready_queue.enqueue(handle);
      }
      else
      {
         m_job_pool.addContinuation(dependency, handle);
      }

      m_pending.fetch_add(1, std::memory_order_release);
   }

   void wait(JobHandle job)
   {
      while (!m_job_pool.finished(job))
      {
         // Work until the the given job is finised
         try_work();
      }
   }

   void waitAll()
   {
      while (m_pending.load(std::memory_order_acquire) > 0)
      {
         // Work until all jobs are done
         try_work();
      }
   }

private:
   moodycamel::BlockingConcurrentQueue<JobHandle> m_ready_queue;
   std::atomic<int> m_pending;

   std::vector<std::thread> m_wokers;

   JobPool m_job_pool;

   void try_work()
   {
      JobHandle job;
      if (m_ready_queue.try_dequeue(job))
      {
         work_one(job);
      }
      else
      {
         std::this_thread::yield();
      }
   }

   void work_one(JobHandle job)
   {
      std::vector<JobHandle> continuations = m_job_pool.invoke(job);
      m_pending.fetch_add(-1, std::memory_order_release);

      m_ready_queue.enqueue_bulk(continuations.data(), continuations.size());
   }
};
