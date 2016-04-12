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

//
// Threads helpers.
//

#ifndef wasm_support_threads_h
#define wasm_support_threads_h

#include <thread>

namespace wasm {

//
// A thread object.
//
// You can only create and destroy these on the main thread.
//

class Thread {
  std::thread::id id;

public:
  Thread();
  ~Thread();

  // called on the right OS thread, set up id
  void prepare();

  // Checks if this is the main thread.
  static bool isMainThread();

  // Returns how many active threads exist. This includes
  // the main thread, so it is always at least 1.
  static size_t getNumThreads();
};

} // namespace wasm

#endif  // wasm_support_threads_h
