#pragma once

#include <thread>
#include <atomic>
#include <functional>
#include <algorithm>

#include "concurrentqueue.h"
#include "blockingconcurrentqueue.h"

class JobPool;

class Job {
public:
   Job(std::function<void()>&& task) : m_task(std::move(task)) {};
   Job() : m_task(nullptr) {};

private:
   friend class JobPool;

   std::function<void()> m_task;
};

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
      handle.version = (*m_version)[next_free].load(std::memory_order_acquire);
      (*m_pool)[next_free] = Job(std::move(task));

      return true;
   }

   // Thread safe for a given handle
   // Should not be called from 2 threads with the same handle.id
   void invoke(JobHandle handle)
   {
      // First run the task associated to this handle
      (*m_pool)[handle.id].m_task();
      
      // Invalidate this job by incrementing the version in the m_version
      (*m_version)[handle.id].fetch_add(1, std::memory_order_release);

      // And add the fact that this id is now available to use by another job
      m_available.enqueue(handle.id);
   }

   // Thread safe
   bool finised(JobHandle handle)
   {
      // A job is finished when it has been released calling invoke
      return handle.version < (*m_version)[handle.id].load(std::memory_order_acquire);
   }

private:
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
      const int thread_count = std::max(static_cast<unsigned int>(1), std::thread::hardware_concurrency() - 1);
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

   JobHandle schedule(std::function<void()>&& task)
   {
      JobHandle handle;
      while (!m_job_pool.create(std::move(task), handle))
      {
         // Work until the job pool can get a new job
         try_work();
      }

      m_ready_queue.enqueue(handle);
      m_pending.fetch_add(1, std::memory_order_release);

      return handle;
   }

   void wait(JobHandle job)
   {
      while (!m_job_pool.finised(job))
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
      m_job_pool.invoke(job);
      m_pending.fetch_add(-1, std::memory_order_release);
   }
};
