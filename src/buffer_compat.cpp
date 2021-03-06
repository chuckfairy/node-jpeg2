#include <node.h>
#include <node_buffer.h>
#include <node_version.h>
#include <v8.h>
#include "buffer_compat.h"

char *BufferData(v8::Local<v8::Object> buf_obj) {
    return node::Buffer::Data(buf_obj);
}


size_t BufferLength(v8::Local<v8::Object> buf_obj) {
    return node::Buffer::Length(buf_obj);
}
