/*
 * Copyright 2016 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>

#include <cstdlib>

#include "threads.h"


namespace wasm {

// Global thread information

std::thread::id mainThreadId;

struct MainThreadNoter {
  MainThreadNoter() {
    // global ctors are called on main thread
    mainThreadId = std::this_thread::get_id();
  }
};

static MainThreadNoter noter;

static ThreadPool* pool = nullptr;


// Thread

Thread::Thread() {
  // main thread object's constructor itself can
  // happen before onMainThread is ready
  assert(onMainThread());
  thread = std::unique_ptr<std::thread>(new std::thread(mainLoop, this));
}

Thread::~Thread() {
  assert(onMainThread());
  done = true;
  {
    std::lock_guard<std::mutex> lock(mutex);
    condition.notify_one();
  }
  thread->join();
}

void Thread::runTasks(std::function<void* ()> getTask_,
                      std::function<void (void*)> runTask_) {
  // TODO: fancy work stealing
  assert(onMainThread());
  getTask = getTask_;
  runTask = runTask_;
  {
    std::lock_guard<std::mutex> lock(mutex);
    condition.notify_one();
  }
}

bool Thread::onMainThread() {
  return std::this_thread::get_id() == mainThreadId;
}

void Thread::mainLoop(void *self_) {
  auto* self = static_cast<Thread*>(self_);
  while (1) {
    {
      std::lock_guard<std::mutex> lock(self->mutex);
      condition.wait(self->lock);//, [&]{ return OptimizerWorker::done || OptimizerWorker::inputs.size() > 0; });
    }
    if (done) break;
    // grab next task
    self->runTask(self->getTask());
  }
}


// ThreadPool

ThreadPool::ThreadPool(size_t num) {
  for (size_t i = 0; i < num; i++) {
    threads[i] = std::unique_ptr<Thread>(new Thread());

  }
}

ThreadPool* ThreadPool::get() {
  if (!pool) {
    size_t num = std::thread::hardware_concurrency();
    if (num < 2) return nullptr;
    pool = new ThreadPool(num);
    atexit([pool]() {
      delete pool;
    });
  }
  return pool;
}

void ThreadPool::runTasks(std::function<void* ()> getTask,
                          std::function<void (void*)> runTask) {
  // TODO: fancy work stealing
  assert(onMainThread());
  assert(!running);
  running = true;
  for (auto thread : threads) {
    thread->runTasks([&]() -> void* {
      std::lock_guard<std::mutex> lock(mutex);
      return getTask();
    }, runTask);
  }
  running = false;
}

bool ThreadPool::isRunning() {
  return pool && pool->running;
}

} // namespace wasm

