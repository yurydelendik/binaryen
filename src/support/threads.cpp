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

#include <iostream>
#include <cstdlib>

#include "threads.h"


namespace wasm {

// Global thread information

bool setMainThreadId = false;
std::thread::id mainThreadId;

struct MainThreadNoter {
  MainThreadNoter() {
    // global ctors are called on main thread
    mainThreadId = std::this_thread::get_id();
    setMainThreadId = true;
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

void Thread::work(std::function<ThreadWorkState ()> doWork_) {
  // TODO: fancy work stealing
  assert(onMainThread());
  doWork = doWork_;
  {
    std::lock_guard<std::mutex> lock(mutex);
    condition.notify_one();
  }
}

bool Thread::onMainThread() {
  // mainThread Id might not be set yet if we are in a global ctor, but
  // that is on the main thread anyhow
  return !setMainThreadId || std::this_thread::get_id() == mainThreadId;
}

void Thread::mainLoop(void *self_) {
  auto* self = static_cast<Thread*>(self_);
  while (1) {
    std::unique_lock<std::mutex> lock(self->mutex);
    if (self->doWork) {
      // run tasks until they are all done
      while (self->doWork() == ThreadWorkState::More) {}
      self->doWork = nullptr;
    }
    ThreadPool::get()->notifyThreadIsReady();
    self->condition.wait(lock);
    if (self->done) break;
  }
}


// ThreadPool

//std::mutex debug;
//#define DEBUG_PRINT(x) { std::lock_guard<std::mutex> lock(debug); x }

ThreadPool::ThreadPool(size_t num) {
  if (num == 1) return; // no multiple cores, don't create threads
  std::unique_lock<std::mutex> lock(mutex);
  resetThreadsAreReady();
  for (size_t i = 0; i < num; i++) {
    threads.emplace_back(std::unique_ptr<Thread>(new Thread()));
  }
  condition.wait(lock, [this]() { return areThreadsReady(); });
}

ThreadPool* ThreadPool::get() {
  if (!pool) {
    assert(Thread::onMainThread());
    size_t num = std::thread::hardware_concurrency();
    if (num < 2) num = 1;
    pool = new ThreadPool(num);
    atexit([&]() {
      delete pool;
      pool = nullptr;
    });
  }
  return pool;
}

void ThreadPool::work(std::vector<std::function<ThreadWorkState ()>>& doWorkers) {
  size_t num = threads.size();
  // If no multiple cores, or on a side thread, do not use worker threads
  if (num == 0 || !Thread::onMainThread()) {
    // just run sequentially
    assert(doWorkers.size() > 0);
    while (doWorkers[0]() == ThreadWorkState::More) {}
    return;
  }
  // run in parallel on threads
  // TODO: fancy work stealing
  assert(doWorkers.size() == num);
  assert(!running);
  running = true;
  std::unique_lock<std::mutex> lock(mutex);
  resetThreadsAreReady();
  for (size_t i = 0; i < num; i++) {
    threads[i]->work(doWorkers[i]);
  }
  condition.wait(lock, [this]() { return areThreadsReady(); });
  running = false;
}

size_t ThreadPool::size() {
  return threads.size();
}

bool ThreadPool::isRunning() {
  return pool && pool->running;
}

void ThreadPool::notifyThreadIsReady() {
  ready.fetch_add(1);
}

void ThreadPool::resetThreadsAreReady() {
  ready.store(0);
}

bool ThreadPool::areThreadsReady() {
  return ready.load() == threads.size();
}

} // namespace wasm

