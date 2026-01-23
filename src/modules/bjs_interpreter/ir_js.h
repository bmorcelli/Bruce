#if !defined(LITE_VERSION) && !defined(DISABLE_INTERPRETER)
#ifndef __IR_JS_H__
#define __IR_JS_H__

#include "helpers_js.h"

extern "C" {
#if !defined(BRUCE_DISABLE_IR)
JSValue native_irTransmitFile(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue native_irTransmit(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue native_irRead(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue native_irReadRaw(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
#else
static inline JSValue native_irTransmitFile(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    (void)ctx;
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_UNDEFINED;
}
static inline JSValue native_irTransmit(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    (void)ctx;
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_UNDEFINED;
}
static inline JSValue native_irRead(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    (void)ctx;
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_UNDEFINED;
}
static inline JSValue native_irReadRaw(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    (void)ctx;
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_UNDEFINED;
}
#endif
}

#endif
#endif
