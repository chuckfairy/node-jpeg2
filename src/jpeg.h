#pragma once

#include <node.h>
#include <node_buffer.h>
#include <node_object_wrap.h>
#include <v8.h>

#include <uv.h>

#include "jpeg_encoder.h"

using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Isolate;
using v8::Value;

class Jpeg : public node::ObjectWrap {
    JpegEncoder jpeg_encoder;

    static void UV_JpegEncode(uv_work_t *req);
    static void UV_JpegEncodeAfter(uv_work_t *req);
public:
    static void Initialize(v8::Handle<v8::Object> target);
    Jpeg(unsigned char *ddata, int wwidth, int hheight, buffer_type bbuf_type);

    void JpegEncodeSync( Isolate * );

    void SetQuality(int q);
    void SetSmoothing(int s);

    static void New(const FunctionCallbackInfo<Value>& args);
    static void JpegEncodeSync(const FunctionCallbackInfo<Value>& args);
    static void JpegEncodeAsync(const FunctionCallbackInfo<Value>& args);
    static void SetQuality(const FunctionCallbackInfo<Value>& args);
    static void SetSmoothing(const FunctionCallbackInfo<Value>& args);
};
