/*
 * Copyright (c) 2012, Ben Noordhuis <info@bnoordhuis.nl>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "compat.h"
#include "compat-inl.h"

#include "node.h"
#include "uv.h"
#include "v8.h"

#include <stdio.h>
#include <stdlib.h>

namespace {

namespace C = ::compat;

typedef enum { STOP, RUN, PAUSE } gc_state_t;

bool trace_gc;
int64_t interval;
v8::Isolate* isolate;
gc_state_t state;
gc_state_t prev_state;
uv_timer_t timer_handle;
uv_check_t check_handle;
uv_prepare_t prepare_handle;

void Trace()
{
  static const char states[3][6] = { "STOP", "RUN", "PAUSE" };
  printf("[idle gc] prev_state=%s state=%s\n",
         states[prev_state],
         states[state]);
}

bool IdleNotification()
{
#if NODE_VERSION_AT_LEAST(0, 12, 0)
  static const int idle_time_in_ms = 5;
  return isolate->IdleNotification(idle_time_in_ms);
#else
  return v8::V8::IdleNotification();
#endif
}

void Timer(uv_timer_t*)
{
  if (IdleNotification()) state = PAUSE;
  if (trace_gc) Trace();
}

void Check(uv_check_t*)
{
  prev_state = state;
}

void Prepare(uv_prepare_t*)
{
  if (state == PAUSE && prev_state == PAUSE) state = RUN;
  if (state == RUN) {
    uv_timer_start(&timer_handle, reinterpret_cast<uv_timer_cb>(Timer),
                   interval, 0);
  }
}

void Stop()
{
  state = STOP;
  uv_timer_stop(&timer_handle);
  uv_check_stop(&check_handle);
  uv_prepare_stop(&prepare_handle);
}

C::ReturnType Stop(const C::ArgumentType& args)
{
  C::ReturnableHandleScope handle_scope(args);
  Stop();
  return handle_scope.Return();
}

C::ReturnType Start(const C::ArgumentType& args)
{
  C::ReturnableHandleScope handle_scope(args);
  Stop();

  interval = args[0]->IsNumber() ? args[0]->IntegerValue() : 0;
  if (interval <= 0) interval = 5000;  // Default to 5 seconds.

  state = RUN;
  uv_check_start(&check_handle, reinterpret_cast<uv_check_cb>(Check));
  uv_prepare_start(&prepare_handle, reinterpret_cast<uv_prepare_cb>(Prepare));

  return handle_scope.Return();
}

void Init(v8::Local<v8::Object> obj)
{
  isolate = v8::Isolate::GetCurrent();

  uv_timer_init(uv_default_loop(), &timer_handle);
  uv_check_init(uv_default_loop(), &check_handle);
  uv_prepare_init(uv_default_loop(), &prepare_handle);
  uv_unref(reinterpret_cast<uv_handle_t*>(&timer_handle));
  uv_unref(reinterpret_cast<uv_handle_t*>(&check_handle));
  uv_unref(reinterpret_cast<uv_handle_t*>(&prepare_handle));

  obj->Set(C::String::NewFromUtf8(isolate, "stop"),
           C::FunctionTemplate::New(isolate, Stop)->GetFunction());
  obj->Set(C::String::NewFromUtf8(isolate, "start"),
           C::FunctionTemplate::New(isolate, Start)->GetFunction());

#if NODE_VERSION_AT_LEAST(1, 0, 0)
  // v8::Isolate::IdleNotification() is a no-op without --use_idle_notification.
  {
    static const char flag[] = "--use_idle_notification";
    v8::V8::SetFlagsFromString(flag, sizeof(flag) - 1);
  }
#endif

  const char* var = getenv("IDLE_GC_TRACE");
  trace_gc = (var != NULL && atoi(var) != 0);
}
NODE_MODULE(idle_gc, Init)

}  // anonymous namespace
