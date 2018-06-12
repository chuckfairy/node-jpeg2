#include <node.h>
#include <node_buffer.h>
#include <jpeglib.h>
#include <cstdlib>
#include <cstring>

#include "common.h"
#include "fixed_jpeg_stack.h"
#include "jpeg_encoder.h"
#include "buffer_compat.h"

using namespace v8;
using namespace node;

void FixedJpegStack::Initialize(v8::Handle<v8::Object> target) {

    Isolate* isolate = target->GetIsolate();

    Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
    tpl->SetClassName(String::NewFromUtf8(isolate, "FixedJpegStack"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(tpl, "encode", JpegEncodeAsync);
    NODE_SET_PROTOTYPE_METHOD(tpl, "encodeSync", JpegEncodeSync);
    NODE_SET_PROTOTYPE_METHOD(tpl, "setQuality", SetQuality);
    NODE_SET_PROTOTYPE_METHOD(tpl, "push", Push);

    target->Set(String::NewFromUtf8(isolate, "FixedJpegStack"), tpl->GetFunction());

}

FixedJpegStack::FixedJpegStack(int wwidth, int hheight, buffer_type bbuf_type) :
    width(wwidth), height(hheight), quality(60), buf_type(bbuf_type)
{

    data = (unsigned char *)calloc(width*height*3, sizeof(*data));
    if (!data) throw "calloc in FixedJpegStack::FixedJpegStack failed!";

}

void FixedJpegStack::JpegEncodeSync( Isolate * isolate ) {

    try {

        JpegEncoder jpeg_encoder(data, width, height, quality, BUF_RGB);
        jpeg_encoder.encode();

        //Buffer send
        int jpeg_len = jpeg_encoder.get_jpeg_len();
        v8::MaybeLocal<v8::Object> retbuf = Buffer::New(isolate, jpeg_len);
        v8::Local<v8::Object> local;
        retbuf.ToLocal( &local );
        memcpy(
            Buffer::Data(local),
            jpeg_encoder.get_jpeg(),
            jpeg_len
        );

        //return scope.Close(retbuf->handle_);

    } catch (const char *err) {
        VException( isolate, err );
    }

}

void FixedJpegStack::Push(unsigned char *data_buf, int x, int y, int w, int h) {

    int start = y*width*3 + x*3;

    switch (buf_type) {
    case BUF_RGB:
        for (int i = 0; i < h; i++) {
            unsigned char *datap = &data[start + i*width*3];
            for (int j = 0; j < w; j++) {
                *datap++ = *data_buf++;
                *datap++ = *data_buf++;
                *datap++ = *data_buf++;
            }
        }
        break;

    case BUF_BGR:
        for (int i = 0; i < h; i++) {
            unsigned char *datap = &data[start + i*width*3];
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
            unsigned char *datap = &data[start + i*width*3];
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
            unsigned char *datap = &data[start + i*width*3];
            for (int j = 0; j < w; j++) {
                *datap++ = *(data_buf+2);
                *datap++ = *(data_buf+1);
                *datap++ = *data_buf;
                data_buf += 4;
            }
        }
        break;

    default:
        throw "Unexpected buf_type in FixedJpegStack::Push";
    }

}


void FixedJpegStack::SetQuality(int q) {

    quality = q;

}

void FixedJpegStack::New(const FunctionCallbackInfo<Value>& args) {

    Isolate* isolate = args.GetIsolate();

    if (args.Length() < 2) {
        VException( isolate, "At least two arguments required - width, height, [and buffer type]" );
        return;
    }

    if (!args[0]->IsInt32()) {
        VException( isolate, "First argument must be integer width." );
        return;
    }

    if (!args[1]->IsInt32()) {
        VException( isolate, "Second argument must be integer height." );
        return;
    }

    int w = args[0]->Int32Value();
    int h = args[1]->Int32Value();

    if (w < 0) {
        VException( isolate, "Width can't be negative." );
        return;
    }

    if (h < 0) {
        VException( isolate, "Height can't be negative." );
        return;
    }

    buffer_type buf_type = BUF_RGB;
    if (args.Length() == 3) {
        if (!args[2]->IsString()) {
            VException( isolate, "Third argument must be a string. Either 'rgb', 'bgr', 'rgba' or 'bgra'." );
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

    try {
        FixedJpegStack *jpeg = new FixedJpegStack( w, h, buf_type );
        jpeg->Wrap(args.This());
        args.This();
    }
    catch (const char *err) {
        VException( isolate, err );
        return;
    }

}

void FixedJpegStack::JpegEncodeSync(const FunctionCallbackInfo<Value>& args) {

    FixedJpegStack *jpeg = ObjectWrap::Unwrap<FixedJpegStack>(args.This());
    jpeg->JpegEncodeSync( args.GetIsolate() );

}

void FixedJpegStack::Push(const FunctionCallbackInfo<Value>& args) {

    Isolate* isolate = args.GetIsolate();

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

    FixedJpegStack *jpeg = ObjectWrap::Unwrap<FixedJpegStack>(args.This());
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

    if (x >= jpeg->width) {
        VException(isolate, "Coordinate x exceeds FixedJpegStack's dimensions.");
    }

    if (y >= jpeg->height) {
        VException(isolate, "Coordinate y exceeds FixedJpegStack's dimensions.");
        return;
    }

    if (x+w > jpeg->width) {
        VException(isolate, "Pushed fragment exceeds FixedJpegStack's width.");
        return;
    }

    if (y+h > jpeg->height) {
        VException(isolate, "Pushed fragment exceeds FixedJpegStack's height.");
        return;
    }

    jpeg->Push((unsigned char *)Buffer::Data(data_buf), x, y, w, h);

    Undefined( isolate );

}

void FixedJpegStack::SetQuality(const FunctionCallbackInfo<Value>& args) {

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

    FixedJpegStack *jpeg = ObjectWrap::Unwrap<FixedJpegStack>(args.This());
    jpeg->SetQuality(q);

    Undefined(isolate);

}

void FixedJpegStack::UV_JpegEncode(uv_work_t *req) {

    encode_request *enc_req = (encode_request *)req->data;
    FixedJpegStack *jpeg = (FixedJpegStack *)enc_req->jpeg_obj;

    try {
        JpegEncoder encoder(jpeg->data, jpeg->width, jpeg->height, jpeg->quality, BUF_RGB);
        encoder.encode();
        enc_req->jpeg_len = encoder.get_jpeg_len();
        enc_req->jpeg = (char *)malloc(sizeof(*enc_req->jpeg)*enc_req->jpeg_len);
        if (!enc_req->jpeg) {
            enc_req->error = strdup("malloc in FixedJpegStack::UV_JpegEncode failed.");
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

void FixedJpegStack::UV_JpegEncodeAfter(uv_work_t *req) {

    encode_request *enc_req = (encode_request *)req->data;
    delete req;

    Handle<Value> argv[2];

    if (enc_req->error) {
        //argv[0] = Undefined();
        argv[1] = ErrorException(enc_req->isolate, enc_req->error);
    }
    else {
        v8::MaybeLocal<v8::Object> bufMaybe = Buffer::New(enc_req->isolate, enc_req->jpeg_len);
        v8::Local<v8::Object> * buf;
        bufMaybe.ToLocal<v8::Object>( buf );
        memcpy(Buffer::Data(*buf), enc_req->jpeg, enc_req->jpeg_len);
        argv[0] = *buf;
        //argv[1] = Undefined();
    }

    TryCatch try_catch( enc_req->isolate ); // don't quite see the necessity of this

    enc_req->callback->Call( Null( enc_req->isolate ), 2, argv );

    if (try_catch.HasCaught()) {
        FatalException( enc_req->isolate, try_catch );
    }

    //enc_req->callback.Dispose();
    free(enc_req->jpeg);
    free(enc_req->error);

    ((FixedJpegStack *)enc_req->jpeg_obj)->Unref();
    free(enc_req);
}

void FixedJpegStack::JpegEncodeAsync(const FunctionCallbackInfo<Value>& args) {

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
    FixedJpegStack *jpeg = ObjectWrap::Unwrap<FixedJpegStack>(args.This());

    encode_request *enc_req = (encode_request *)malloc(sizeof(*enc_req));

    if (!enc_req) {
        VException(isolate, "malloc in FixedJpegStack::JpegEncodeAsync failed.");
        return;
    }

    enc_req->callback = callback;
    enc_req->isolate = isolate;
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
