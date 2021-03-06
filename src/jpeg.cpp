#include <node.h>
#include <node_buffer.h>
#include <jpeglib.h>
#include <cstdlib>
#include <cstring>

#include "common.h"
#include "jpeg.h"
#include "jpeg_encoder.h"
#include "buffer_compat.h"

using namespace v8;
using namespace node;

using v8::Function;
using v8::Integer;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Isolate;
using v8::Value;

void Jpeg::Initialize(v8::Handle<v8::Object> target) {

    //Local<FunctionTemplate> t = FunctionTemplate::New(New);
    //t->InstanceTemplate()->SetInternalFieldCount(1);
    Isolate* isolate = target->GetIsolate();

    Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
    tpl->SetClassName(String::NewFromUtf8(isolate, "Jpeg"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(tpl, "encode", JpegEncodeAsync);
    NODE_SET_PROTOTYPE_METHOD(tpl, "encodeSync", JpegEncodeSync);
    NODE_SET_PROTOTYPE_METHOD(tpl, "setQuality", SetQuality);
    NODE_SET_PROTOTYPE_METHOD(tpl, "setSmoothing", SetSmoothing);

    target->Set(String::NewFromUtf8(isolate, "Jpeg"), tpl->GetFunction());

};

Jpeg::Jpeg(unsigned char *ddata, int wwidth, int hheight, buffer_type bbuf_type) :
    jpeg_encoder(ddata, wwidth, hheight, 60, bbuf_type) {}

void Jpeg::JpegEncodeSync( Isolate * isolate ) {

    try {
        jpeg_encoder.encode();
    } catch( const char *err ) {
        VException( isolate, err );
        return;
    }

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

}

void Jpeg::SetQuality(int q) {

    jpeg_encoder.set_quality(q);

}

void Jpeg::SetSmoothing(int s) {

    jpeg_encoder.set_smoothing(s);

}

void Jpeg::New(const FunctionCallbackInfo<Value>& args) {

    Isolate* isolate = args.GetIsolate();

    if (args.Length() < 3) {
        VException( isolate, "At least three arguments required - buffer, width, height, [and buffer type]" );
        return;
    }

    if (!Buffer::HasInstance(args[0])) {
        VException( isolate, "First argument must be Buffer." );
        return ;
    }

    if (!args[1]->IsInt32()) {
        VException( isolate, "Second argument must be integer width." );
        return;
    }

    if (!args[2]->IsInt32()) {
        VException( isolate, "Third argument must be integer height." );
        return;
    }

    int w = args[1]->Int32Value();
    int h = args[2]->Int32Value();

    if (w < 0) {
        VException( isolate, "Width can't be negative." );
        return;
    }

    if (h < 0) {
        VException( isolate, "Height can't be negative." );
        return;
    }

    buffer_type buf_type = BUF_RGB;

    if (args.Length() == 4) {

        if (!args[3]->IsString()) {
            VException( isolate, "Fifth argument must be a string. Either 'rgb', 'bgr', 'rgba' or 'bgra'.");
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

    Local<Object> buffer = args[0]->ToObject();
    Jpeg *jpeg = new Jpeg((unsigned char*) Buffer::Data(buffer), w, h, buf_type);
    jpeg->Wrap(args.This());

}

void Jpeg::JpegEncodeSync(const FunctionCallbackInfo<Value>& args)
{
    Jpeg *jpeg = ObjectWrap::Unwrap<Jpeg>(args.This());
    jpeg->JpegEncodeSync( args.GetIsolate() );
}

void Jpeg::SetQuality(const FunctionCallbackInfo<Value>& args) {

    Isolate* isolate = args.GetIsolate();

    if (args.Length() != 1) {
        VException( isolate, "One argument required - quality" );
        return;
    }

    if (!args[0]->IsInt32()) {
        VException( isolate, "First argument must be integer quality" );
        return;
    }

    int q = args[0]->Int32Value();

    if (q < 0) {
        VException( isolate, "Quality must be greater or equal to 0." );
        return;
    }

    if (q > 100) {
        VException( isolate, "Quality must be less than or equal to 100." );
        return;
    }

    Jpeg *jpeg = ObjectWrap::Unwrap<Jpeg>(args.This());
    jpeg->SetQuality(q);

    Undefined( isolate );

}

void Jpeg::SetSmoothing(const FunctionCallbackInfo<Value>& args)
{
    Isolate* isolate = args.GetIsolate();

    if (args.Length() != 1) {
        VException( isolate, "One argument required - quality");
        return;
    }

    if (!args[0]->IsInt32()) {
        VException( isolate, "First argument must be integer quality" );
        return;
    }

    int s = args[0]->NumberValue();

    if (s < 0) {
        VException( isolate, "Smoothing must be greater or equal to 0." );
        return;
    }

    if (s > 100) {
        VException( isolate, "Smoothing must be less than or equal to 100." );
        return;
    }

    Jpeg *jpeg = node::ObjectWrap::Unwrap<Jpeg>(args.This());
    jpeg->SetSmoothing(s);

    Undefined( isolate );
}

void Jpeg::UV_JpegEncode(uv_work_t *req) {
    encode_request *enc_req = (encode_request *)req->data;
    Jpeg *jpeg = (Jpeg *)enc_req->jpeg_obj;

    try {
        jpeg->jpeg_encoder.encode();
        enc_req->jpeg_len = jpeg->jpeg_encoder.get_jpeg_len();
        enc_req->jpeg = (char *)malloc(sizeof(*enc_req->jpeg)*enc_req->jpeg_len);
        if (!enc_req->jpeg) {
            enc_req->error = strdup("malloc in Jpeg::UV_JpegEncode failed.");
            return;
        }
        else {
            memcpy(enc_req->jpeg, jpeg->jpeg_encoder.get_jpeg(), enc_req->jpeg_len);
        }
    }
    catch (const char *err) {
        enc_req->error = strdup(err);
    }
}

void Jpeg::UV_JpegEncodeAfter(uv_work_t *req) {

    encode_request *enc_req = (encode_request *)req->data;
    delete req;

    Handle<Value> argv[2];

    if (enc_req->error) {
        //argv[0] = 0;
        //argv[1] = ErrorException(enc_req->error);
    } else {
        v8::MaybeLocal<v8::Object> bufMaybe = Buffer::New(enc_req->isolate, enc_req->jpeg_len);
        v8::Local<v8::Object> * buf;
        bufMaybe.ToLocal<v8::Object>( buf );
        memcpy(Buffer::Data(*buf), enc_req->jpeg, enc_req->jpeg_len);
        argv[0] = *buf;
        //argv[1] = 0;
    }

    TryCatch try_catch( enc_req->isolate ); // don't quite see the necessity of this

    enc_req->callback->Call( Null( enc_req->isolate ), 2, argv );

    if (try_catch.HasCaught()) {
        FatalException( enc_req->isolate, try_catch );
    }

    //enc_req->callback.Dispose();
    free(enc_req->jpeg);
    free(enc_req->error);

    ((Jpeg *)enc_req->jpeg_obj)->Unref();
    free(enc_req);
}

void Jpeg::JpegEncodeAsync(const FunctionCallbackInfo<Value>& args) {

    Isolate* isolate = args.GetIsolate();

    if (args.Length() != 1) {
        VException( isolate, "One argument required - callback function." );
        return;
    }

    if (!args[0]->IsFunction()) {
        VException( isolate, "First argument must be a function." );
        return;
    }

    Local<Function> callback = Local<Function>::Cast(args[0]);
    Jpeg *jpeg = ObjectWrap::Unwrap<Jpeg>(args.This());

    encode_request  *enc_req = (encode_request *)malloc(sizeof(*enc_req));

    if( ! enc_req ) {
        VException( isolate, "malloc in Jpeg::JpegEncodeAsync failed." );
        return;
    }

    //enc_req->callback = Persistent<Function>::Persistent( isolate, callback );
    enc_req->callback = callback;
    enc_req->jpeg_obj = jpeg;
    enc_req->isolate = isolate;
    enc_req->jpeg = NULL;
    enc_req->jpeg_len = 0;
    enc_req->error = NULL;

    uv_work_t* req = new uv_work_t;
    req->data = enc_req;

    uv_queue_work(uv_default_loop(), req, UV_JpegEncode, (uv_after_work_cb)UV_JpegEncodeAfter);

    jpeg->Ref();

    Undefined( isolate );

}
