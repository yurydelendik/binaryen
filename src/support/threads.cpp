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

#include "support/threads.h"


namespace wasm {

// Global thread information

static size_t numThreads = 0;

// Represents the main thread, ensures that numThreads is initially 1.
static Thread mainThread;

struct MainThreadPreparer {
  MainThreadPreparer() {
    // global ctors are called on main thread
    mainThread.prepare();
  }
};

static MainThreadPreparer preparer;

Thread::Thread() {
  // main thread object's constructor itself can
  // happen before isMainThread is ready
  assert(this == &mainThread || isMainThread());
  numThreads++;
}

Thread::~Thread() {
  assert(this == &mainThread || isMainThread());
  numThreads--;
}

void Thread::prepare() {
  id = std::this_thread::get_id();
}

bool Thread::isMainThread() {
  return std::this_thread::get_id() == mainThread.id;
}

size_t Thread::getNumThreads() {
  // during startup, numThreads may not be bumped to 1 yet
  return numThreads > 0 ? numThreads : 1;
}

} // namespace wasm

