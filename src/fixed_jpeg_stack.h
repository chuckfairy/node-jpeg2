#ifndef STACKED_JPEG_H
#define STACKED_JPEG_H

#include <node.h>
#include <node_buffer.h>
#include <node_object_wrap.h>

#include <uv.h>

#include "common.h"
#include "jpeg_encoder.h"


using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Isolate;
using v8::Value;

class FixedJpegStack : public node::ObjectWrap {
    int width, height, quality;
    buffer_type buf_type;

    unsigned char *data;

    static void UV_JpegEncode(uv_work_t *req);
    static void UV_JpegEncodeAfter(uv_work_t *req);

public:
    static void Initialize(v8::Handle<v8::Object> target);

    FixedJpegStack(int wwidth, int hheight, buffer_type bbuf_type);

    void JpegEncodeSync( Isolate * isolate );

    void Push(unsigned char *data_buf, int x, int y, int w, int h);

    void SetQuality(int q);

    static void New(const FunctionCallbackInfo<Value>& args);
    static void JpegEncodeSync(const FunctionCallbackInfo<Value>& args);
    static void JpegEncodeAsync(const FunctionCallbackInfo<Value>& args);
    static void Push(const FunctionCallbackInfo<Value>& args);
    static void SetQuality(const FunctionCallbackInfo<Value>& args);

};

#endif

