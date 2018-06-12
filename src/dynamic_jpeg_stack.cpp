#include <node.h>
#include <node_buffer.h>
#include <jpeglib.h>
#include <cstdlib>
#include <cstring>

#include "common.h"
#include "dynamic_jpeg_stack.h"
#include "jpeg_encoder.h"
#include "buffer_compat.h"

using namespace v8;
using namespace node;

void
DynamicJpegStack::Initialize(v8::Handle<v8::Object> target)
{

    Isolate* isolate = target->GetIsolate();

    Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
    tpl->SetClassName(String::NewFromUtf8(isolate, "DynamicJpegStack"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    Local<FunctionTemplate> t = FunctionTemplate::New(New);
    t->InstanceTemplate()->SetInternalFieldCount(1);
    NODE_SET_PROTOTYPE_METHOD(tpl, "encode", JpegEncodeAsync);
    NODE_SET_PROTOTYPE_METHOD(tpl, "encodeSync", JpegEncodeSync);
    NODE_SET_PROTOTYPE_METHOD(tpl, "push", Push);
    NODE_SET_PROTOTYPE_METHOD(tpl, "reset", Reset);
    NODE_SET_PROTOTYPE_METHOD(tpl, "setBackground", SetBackground);
    NODE_SET_PROTOTYPE_METHOD(tpl, "setQuality", SetQuality);
    NODE_SET_PROTOTYPE_METHOD(tpl, "dimensions", Dimensions);

    target->Set(String::NewSymbol("DynamicJpegStack"), t->GetFunction());

}

DynamicJpegStack::DynamicJpegStack(buffer_type bbuf_type) :
    quality(60), buf_type(bbuf_type),
    dyn_rect(-1, -1, 0, 0),
    bg_width(0), bg_height(0), data(NULL) {}

DynamicJpegStack::~DynamicJpegStack()
{
    free(data);
}

void DynamicJpegStack::update_optimal_dimension(int x, int y, int w, int h) {

    if (dyn_rect.x == -1 || x < dyn_rect.x)
        dyn_rect.x = x;
    if (dyn_rect.y == -1 || y < dyn_rect.y)
        dyn_rect.y = y;

    if (dyn_rect.w == 0)
        dyn_rect.w = w;
    if (dyn_rect.h == 0)
        dyn_rect.h = h;

    int ww = w - (dyn_rect.w - (x - dyn_rect.x));
    if (ww > 0)
        dyn_rect.w += ww;

    int hh = h - (dyn_rect.h - (y - dyn_rect.y));
    if (hh > 0)
        dyn_rect.h += hh;
}

void DynamicJpegStack::JpegEncodeSync() {

    try {
        JpegEncoder jpeg_encoder(data, bg_width, bg_height, quality, BUF_RGB);
        jpeg_encoder.setRect(Rect(dyn_rect.x, dyn_rect.y, dyn_rect.w, dyn_rect.h));
        jpeg_encoder.encode();
        int jpeg_len = jpeg_encoder.get_jpeg_len();
        Buffer *retbuf = Buffer::New(jpeg_len);
        memcpy(Buffer::Data(retbuf), jpeg_encoder.get_jpeg(), jpeg_len);
        //return scope.Close(retbuf->handle_);
    }
    catch (const char *err) {
        return VException(err);
    }
}

void DynamicJpegStack::Push(unsigned char *data_buf, int x, int y, int w, int h) {

    update_optimal_dimension(x, y, w, h);

    int start = y*bg_width*3 + x*3;

    switch (buf_type) {
    case BUF_RGB:
        for (int i = 0; i < h; i++) {
            unsigned char *datap = &data[start + i*bg_width*3];
            for (int j = 0; j < w; j++) {
                *datap++ = *data_buf++;
                *datap++ = *data_buf++;
                *datap++ = *data_buf++;
            }
        }
        break;

    case BUF_BGR:
        for (int i = 0; i < h; i++) {
            unsigned char *datap = &data[start + i*bg_width*3];
            for (int j = 0; j < w; j++) {
                *datap++ = *(data_buf+2);
                *datap++ = *(data_buf+1);
                *datap++ = *data_buf;
                data_buf+=3;
            }
        }
        break;

    case BUF_RGBA:
        for (int i = 0; i < h; i++) {
            unsigned char *datap = &data[start + i*bg_width*3];
            for (int j = 0; j < w; j++) {
                *datap++ = *data_buf++;
                *datap++ = *data_buf++;
                *datap++ = *data_buf++;
                data_buf++;
            }
        }
        break;

    case BUF_BGRA:
        for (int i = 0; i < h; i++) {
            unsigned char *datap = &data[start + i*bg_width*3];
            for (int j = 0; j < w; j++) {
                *datap++ = *(data_buf+2);
                *datap++ = *(data_buf+1);
                *datap++ = *data_buf;
                data_buf += 4;
            }
        }
        break;

    default:
        throw "Unexpected buf_type in DynamicJpegStack::Push";
    }
}

void DynamicJpegStack::SetBackground(unsigned char *data_buf, int w, int h) {

    if (data) {
        free(data);
        data = NULL;
    }

    switch (buf_type) {
    case BUF_RGB:
        data = (unsigned char *)malloc(sizeof(*data)*w*h*3);
        if (!data) throw "malloc failed in DynamicJpegStack::SetBackground";
        memcpy(data, data_buf, w*h*3);
        break;

    case BUF_BGR:
        data = bgr_to_rgb(data_buf, w*h*3);
        if (!data) throw "malloc failed in DynamicJpegStack::SetBackground";
        break;

    case BUF_RGBA:
        data = rgba_to_rgb(data_buf, w*h*4);
        if (!data) throw "malloc failed in DynamicJpegStack::SetBackground";
        break;

    case BUF_BGRA:
        data = bgra_to_rgb(data_buf, w*h*4);
        if (!data) throw "malloc failed in DynamicJpegStack::SetBackground";
        break;

    default:
        throw "Unexpected buf_type in DynamicJpegStack::SetBackground";
    }
    bg_width = w;
    bg_height = h;
}

void DynamicJpegStack::SetQuality(int q) {
    quality = q;
}

void DynamicJpegStack::Reset() {
    dyn_rect = Rect(-1, -1, 0, 0);
}

v8::Handle<v8::Value> DynamicJpegStack::Dimensions() {

    Local<Object> dim = Object::New();
    dim->Set(String::NewSymbol("x"), Integer::New(dyn_rect.x));
    dim->Set(String::NewSymbol("y"), Integer::New(dyn_rect.y));
    dim->Set(String::NewSymbol("width"), Integer::New(dyn_rect.w));
    dim->Set(String::NewSymbol("height"), Integer::New(dyn_rect.h));

    //return scope.Close(dim);
}

void DynamicJpegStack::New(const FunctionCallbackInfo<Value>& args) {

    Isolate* isolate = args.GetIsolate();

    if (args.Length() > 1) {
        VException(isolate, "One argument max - buffer type.");
        return;
    }

    buffer_type buf_type = BUF_RGB;
    if (args.Length() == 1) {
        if (!args[0]->IsString()) {
            VException(isolate, "First argument must be a string. Either 'rgb', 'bgr', 'rgba' or 'bgra'.");
            return;
        }

        String::Utf8Value str(args[3]);
        const char * bt = * str;

        if (!(str_eq(bt, "rgb")
            || str_eq(bt, "bgr")
            || str_eq(bt, "rgba")
            || str_eq(bt, "bgra"))
        ) {
            VException( isolate, "Buffer type must be 'rgb', 'bgr', 'rgba' or 'bgra'." );
            return;
        }

        if (str_eq(bt, "rgb")) {
            buf_type = BUF_RGB;
        } else if (str_eq(bt, "bgr")) {
            buf_type = BUF_BGR;
        } else if (str_eq(bt, "rgba")) {
            buf_type = BUF_RGBA;
        } else if (str_eq(bt, "bgra")) {
            buf_type = BUF_BGRA;
        } else {
            VException( isolate, "Buffer type wasn't 'rgb', 'bgr', 'rgba' or 'bgra'." );
            return;
        }

    }

    DynamicJpegStack *jpeg = new DynamicJpegStack(buf_type);
    jpeg->Wrap(args.This());

    //return
    args.This();

}

void DynamicJpegStack::JpegEncodeSync(const FunctionCallbackInfo<Value>& args) {
    DynamicJpegStack *jpeg = ObjectWrap::Unwrap<DynamicJpegStack>(args.This());
    //return scope.Close(jpeg->JpegEncodeSync());
}

void DynamicJpegStack::Push(const FunctionCallbackInfo<Value>& args) {

    Isolate* isolate = args.GetIsolate();

    if (args.Length() != 5) {
        VException(isolate, "Five arguments required - buffer, x, y, width, height.");
        return;
    }

    if (!Buffer::HasInstance(args[0])) {
        VException(isolate, "First argument must be Buffer.");
        return;
    }

    if (!args[1]->IsInt32()) {
        VException(isolate, "Second argument must be integer x.");
        return;
    }

    if (!args[2]->IsInt32()) {
        VException(isolate, "Third argument must be integer y.");
        return;
    }

    if (!args[3]->IsInt32()) {
        VException(isolate, "Fourth argument must be integer w.");
        return;
    }

    if (!args[4]->IsInt32()) {
        VException(isolate, "Fifth argument must be integer h.");
        return;
    }

    DynamicJpegStack *jpeg = ObjectWrap::Unwrap<DynamicJpegStack>(args.This());

    if (!jpeg->data) {
        VException(
            isolate,
            "No background has been set, use setBackground or setSolidBackground to set."
        );
        return;
    }

    Local<Object> data_buf = args[0]->ToObject();
    int x = args[1]->Int32Value();
    int y = args[2]->Int32Value();
    int w = args[3]->Int32Value();
    int h = args[4]->Int32Value();

    if (x < 0) {
        VException(isolate, "Coordinate x smaller than 0.");
        return;
    }

    if (y < 0) {
        VException(isolate, "Coordinate y smaller than 0.");
        return;
    }

    if (w < 0) {
        VException(isolate, "Width smaller than 0.");
        return;
    }

    if (h < 0) {
        VException(isolate, "Height smaller than 0.");
        return;
    }

    if (x >= jpeg->bg_width) {
        VException(isolate, "Coordinate x exceeds DynamicJpegStack's background dimensions.");
        return;
    }

    if (y >= jpeg->bg_height) {
        VException(isolate, "Coordinate y exceeds DynamicJpegStack's background dimensions.");
    }

    if (x+w > jpeg->bg_width) {
        VException(isolate, "Pushed fragment exceeds DynamicJpegStack's width.");
        return;
    }

    if (y+h > jpeg->bg_height) {
        VException(isolate, "Pushed fragment exceeds DynamicJpegStack's height.");
        return;
    }

    jpeg->Push((unsigned char *)Buffer::Data(data_buf), x, y, w, h);

    Undefined( isolate );
}

void DynamicJpegStack::SetBackground(const FunctionCallbackInfo<Value>& args) {

    Isolate* isolate = args.GetIsolate();

    if (args.Length() != 3) {
        VException(isolate, "Four arguments required - buffer, width, height");
        return;
    }

    if (!Buffer::HasInstance(args[0])) {
        VException(isolate, "First argument must be Buffer.");
        return;
    }

    if (!args[1]->IsInt32()) {
        VException(isolate, "Second argument must be integer width.");
        return;
    }

    if (!args[2]->IsInt32()) {
        VException(isolate, "Third argument must be integer height.");
        return;
    }

    DynamicJpegStack *jpeg = ObjectWrap::Unwrap<DynamicJpegStack>(args.This());
    Local<Object> data_buf = args[0]->ToObject();
    int w = args[1]->Int32Value();
    int h = args[2]->Int32Value();

    if (w < 0) {
        VException(isolate, "Coordinate x smaller than 0.");
        return;
    }

    if (h < 0) {
        VException(isolate, "Coordinate y smaller than 0.");
    }

    try {
        jpeg->SetBackground((unsigned char *)Buffer::Data(data_buf), w, h);
    }
    catch (const char *err) {
        VException(isolate, err);
        return;
    }

    Undefined( isolate );

}

void DynamicJpegStack::Reset(const FunctionCallbackInfo<Value>& args) {

    Isolate* isolate = args.GetIsolate();

    DynamicJpegStack *jpeg = ObjectWrap::Unwrap<DynamicJpegStack>(args.This());
    jpeg->Reset();

    Undefined( isolate );

}

void DynamicJpegStack::Dimensions(const FunctionCallbackInfo<Value>& args) {

    DynamicJpegStack *jpeg = ObjectWrap::Unwrap<DynamicJpegStack>(args.This());
    //return scope.Close(jpeg->Dimensions());

}

void DynamicJpegStack::SetQuality(const FunctionCallbackInfo<Value>& args) {

    Isolate* isolate = args.GetIsolate();

    if (args.Length() != 1) {
        VException(isolate, "One argument required - quality");
        return;
    }

    if (!args[0]->IsInt32()) {
        VException(isolate, "First argument must be integer quality");
        return;
    }

    int q = args[0]->Int32Value();

    if (q < 0) {
        VException(isolate, "Quality must be greater or equal to 0.");
        return;
    }
    if (q > 100) {
        VException(isolate, "Quality must be less than or equal to 100.");
        return;
    }

    DynamicJpegStack *jpeg = ObjectWrap::Unwrap<DynamicJpegStack>(args.This());
    jpeg->SetQuality(q);

    Undefined( isolate );
}

void DynamicJpegStack::UV_JpegEncode(uv_work_t *req) {

    encode_request *enc_req = (encode_request *)req->data;
    DynamicJpegStack *jpeg = (DynamicJpegStack *)enc_req->jpeg_obj;

    try {
        Rect &dyn_rect = jpeg->dyn_rect;
        JpegEncoder encoder(jpeg->data, jpeg->bg_width, jpeg->bg_height, jpeg->quality, BUF_RGB);
        encoder.setRect(Rect(dyn_rect.x, dyn_rect.y, dyn_rect.w, dyn_rect.h));
        encoder.encode();
        enc_req->jpeg_len = encoder.get_jpeg_len();
        enc_req->jpeg = (char *)malloc(sizeof(*enc_req->jpeg)*enc_req->jpeg_len);
        if (!enc_req->jpeg) {
            enc_req->error = strdup("malloc in DynamicJpegStack::UV_JpegEncode failed.");
            return;
        }
        else {
            memcpy(enc_req->jpeg, encoder.get_jpeg(), enc_req->jpeg_len);
        }
    }
    catch (const char *err) {
        enc_req->error = strdup(err);
    }
}

void DynamicJpegStack::UV_JpegEncodeAfter(uv_work_t *req) {

    encode_request *enc_req = (encode_request *)req->data;
    delete req;
    DynamicJpegStack *jpeg = (DynamicJpegStack *)enc_req->jpeg_obj;

    Handle<Value> argv[3];

    if (enc_req->error) {
        argv[0] = Undefined( enc_req->isolate );
        argv[1] = Undefined( enc_req->isolate );
        argv[2] = ErrorException(enc_req->isolate, enc_req->error);
    }
    else {
        Buffer *buf = Buffer::New(enc_req->jpeg_len);
        memcpy(Buffer::Data(buf), enc_req->jpeg, enc_req->jpeg_len);
        argv[0] = buf->handle_;
        argv[1] = jpeg->Dimensions();
        argv[2] = Undefined( enc_req->isolate );
    }

    TryCatch try_catch( enc_req->isolate ); // don't quite see the necessity of this

    enc_req->callback->Call( Null( enc_req->isolate ), 2, argv );

    if (try_catch.HasCaught()) {
        FatalException( enc_req->isolate, try_catch );
    }

    //enc_req->callback.Dispose();
    free(enc_req->jpeg);
    free(enc_req->error);

    jpeg->Unref();
    free(enc_req);
}

void DynamicJpegStack::JpegEncodeAsync(const FunctionCallbackInfo<Value>& args) {

    Isolate* isolate = args.GetIsolate();

    if (args.Length() != 1) {
        VException(isolate, "One argument required - callback function.");
        return;
    }

    if (!args[0]->IsFunction()) {
        VException(isolate, "First argument must be a function.");
        return;
    }

    Local<Function> callback = Local<Function>::Cast(args[0]);
    DynamicJpegStack *jpeg = ObjectWrap::Unwrap<DynamicJpegStack>(args.This());

    encode_request *enc_req = (encode_request *)malloc(sizeof(*enc_req));

    if (!enc_req) {
        VException(isolate, "malloc in DynamicJpegStack::JpegEncodeAsync failed.");
        return;
    }

    enc_req->callback = Persistent<Function>::New(callback);
    enc_req->jpeg_obj = jpeg;
    enc_req->jpeg = NULL;
    enc_req->jpeg_len = 0;
    enc_req->error = NULL;

    uv_work_t* req = new uv_work_t;
    req->data = enc_req;
    uv_queue_work(uv_default_loop(), req, UV_JpegEncode, (uv_after_work_cb)UV_JpegEncodeAfter);

    jpeg->Ref();

    Undefined( isolate );

}
