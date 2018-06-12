#ifndef DYNAMIC_JPEG_H
#define DYNAMIC_JPEG_H

#include <node.h>
#include <node_buffer.h>
#include <node_object_wrap.h>

#include <uv.h>

#include <vector>
#include <utility>

#include "common.h"
#include "jpeg_encoder.h"

using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Isolate;
using v8::Value;

class DynamicJpegStack : public node::ObjectWrap {
    int quality;
    buffer_type buf_type;

    unsigned char *data;

    int bg_width, bg_height; // background width and height after setBackground
    Rect dyn_rect; // rect of dynamic push area (updated after each push)

    void update_optimal_dimension(int x, int y, int w, int h);

    static void UV_JpegEncode(uv_work_t *req);
    static void UV_JpegEncodeAfter(uv_work_t *req);

public:

    DynamicJpegStack(buffer_type bbuf_type);
    ~DynamicJpegStack();

    void JpegEncodeSync();
    void Push(unsigned char *data_buf, int x, int y, int w, int h);
    void SetBackground(unsigned char *data_buf, int w, int h);
    void SetQuality(int q);
    v8::Handle<v8::Value> Dimensions();
    void Reset();

    static void Initialize(v8::Handle<v8::Object> target);

    static void New(const FunctionCallbackInfo<Value>& args);
    static void JpegEncodeSync(const FunctionCallbackInfo<Value>& args);
    static void JpegEncodeAsync(const FunctionCallbackInfo<Value>& args);
    static void Push(const FunctionCallbackInfo<Value>& args);
    static void SetBackground(const FunctionCallbackInfo<Value>& args);
    static void SetQuality(const FunctionCallbackInfo<Value>& args);
    static void Dimensions(const FunctionCallbackInfo<Value>& args);
    static void Reset(const FunctionCallbackInfo<Value>& args);
};

#endif

