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

#include "node.h"
#include "uv.h"
#include "v8.h"

#include <stdio.h>
#include <stdlib.h>

namespace {

using v8::Arguments;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Object;
using v8::String;
using v8::Undefined;
using v8::V8;
using v8::Value;

typedef enum { STOP, RUN, PAUSE } gc_state_t;

bool trace_gc;
int64_t interval;
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

void Timer(uv_timer_t*, int)
{
  if (V8::IdleNotification()) state = PAUSE;
  if (trace_gc) Trace();
}

void Check(uv_check_t*, int)
{
  prev_state = state;
}

void Prepare(uv_prepare_t*, int)
{
  if (state == PAUSE && prev_state == PAUSE) state = RUN;
  if (state == RUN) uv_timer_start(&timer_handle, Timer, interval, 0);
}

Handle<Value> Stop(const Arguments& args)
{
  state = STOP;
  uv_timer_stop(&timer_handle);
  uv_check_stop(&check_handle);
  uv_prepare_stop(&prepare_handle);
  return Undefined();
}

Handle<Value> Start(const Arguments& args)
{
  HandleScope scope;
  Stop(args);

  interval = args[0]->IsNumber() ? args[0]->IntegerValue() : 0;
  if (interval <= 0) interval = 5000;  // Default to 5 seconds.

  state = RUN;
  uv_check_start(&check_handle, Check);
  uv_prepare_start(&prepare_handle, Prepare);

  return Undefined();
}

void Init(Handle<Object> obj)
{
  HandleScope scope;

  uv_timer_init(uv_default_loop(), &timer_handle);
  uv_check_init(uv_default_loop(), &check_handle);
  uv_prepare_init(uv_default_loop(), &prepare_handle);
  uv_unref(reinterpret_cast<uv_handle_t*>(&timer_handle));
  uv_unref(reinterpret_cast<uv_handle_t*>(&check_handle));
  uv_unref(reinterpret_cast<uv_handle_t*>(&prepare_handle));

  obj->Set(String::New("stop"), FunctionTemplate::New(Stop)->GetFunction());
  obj->Set(String::New("start"), FunctionTemplate::New(Start)->GetFunction());

  const char* var = getenv("IDLE_GC_TRACE");
  trace_gc = (var != NULL && atoi(var) != 0);
}
NODE_MODULE(idle_gc, Init)

}  // anonymous namespace
