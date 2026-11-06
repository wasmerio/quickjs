#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quickjs.h"
#include "quickjs-libc.h"
#include "cutils.h"

static JSRuntime *new_runtime(void)
{
    JSRuntime *rt = JS_NewRuntime();

    if (rt)
        JS_SetDumpFlags(rt, JS_ABORT_ON_LEAKS);
    return rt;
}

static JSValue eval(JSContext *ctx, const char *code)
{
    return JS_Eval(ctx, code, strlen(code), "<input>", JS_EVAL_TYPE_GLOBAL);
}

static JSValue cfunc_callback(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    return JS_ThrowTypeError(ctx, "from cfunc");
}

static JSValue cfuncdata_callback(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv,
                                  int magic, JSValueConst *func_data)
{
    return JS_ThrowTypeError(ctx, "from cfuncdata");
}

static JSValue cclosure_callback(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv,
                                 int magic, void *func_data)
{
  return JS_ThrowTypeError(ctx, "from cclosure");
}

static bool closure_finalized = false;

static void cclosure_opaque_finalize(void *opaque)
{
    if ((intptr_t)opaque == 12)
        closure_finalized = true;
}

static void cfunctions(void)
{
    uint32_t length;
    const char *s;
    JSValue ret, stack;

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue cfunc = JS_NewCFunction(ctx, cfunc_callback, "cfunc", 42);
    JSValue cfuncdata =
        JS_NewCFunctionData2(ctx, cfuncdata_callback, "cfuncdata",
                             /*length*/1337, /*magic*/0, /*data_len*/0, NULL);
    JSValue cclosure =
        JS_NewCClosure(ctx, cclosure_callback, "cclosure", cclosure_opaque_finalize,
                       /*length*/0xC0DE, /*magic*/11, /*opaque*/(void*)12);
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "cfunc", cfunc);
    JS_SetPropertyStr(ctx, global, "cfuncdata", cfuncdata);
    JS_SetPropertyStr(ctx, global, "cclosure", cclosure);
    JS_FreeValue(ctx, global);

    ret = eval(ctx, "cfunc.name");
    assert(!JS_IsException(ret));
    assert(JS_IsString(ret));
    s = JS_ToCString(ctx, ret);
    JS_FreeValue(ctx, ret);
    assert(s);
    assert(!strcmp(s, "cfunc"));
    JS_FreeCString(ctx, s);
    ret = eval(ctx, "cfunc.length");
    assert(!JS_IsException(ret));
    assert(JS_IsNumber(ret));
    assert(0 == JS_ToUint32(ctx, &length, ret));
    assert(length == 42);

    ret = eval(ctx, "cfuncdata.name");
    assert(!JS_IsException(ret));
    assert(JS_IsString(ret));
    s = JS_ToCString(ctx, ret);
    JS_FreeValue(ctx, ret);
    assert(s);
    assert(!strcmp(s, "cfuncdata"));
    JS_FreeCString(ctx, s);
    ret = eval(ctx, "cfuncdata.length");
    assert(!JS_IsException(ret));
    assert(JS_IsNumber(ret));
    assert(0 == JS_ToUint32(ctx, &length, ret));
    assert(length == 1337);

    ret = eval(ctx, "cclosure.name");
    assert(!JS_IsException(ret));
    assert(JS_IsString(ret));
    s = JS_ToCString(ctx, ret);
    JS_FreeValue(ctx, ret);
    assert(s);
    assert(!strcmp(s, "cclosure"));
    JS_FreeCString(ctx, s);
    ret = eval(ctx, "cclosure.length");
    assert(!JS_IsException(ret));
    assert(JS_IsNumber(ret));
    assert(0 == JS_ToUint32(ctx, &length, ret));
    assert(length == 0xC0DE);

    ret = eval(ctx, "cfunc()");
    assert(JS_IsException(ret));
    ret = JS_GetException(ctx);
    assert(JS_IsError(ret));
    stack = JS_GetPropertyStr(ctx, ret, "stack");
    assert(JS_IsString(stack));
    s = JS_ToCString(ctx, stack);
    JS_FreeValue(ctx, stack);
    assert(s);
    assert(!strcmp(s, "    at cfunc (native)\n    at <eval> (<input>:1:1)\n"));
    JS_FreeCString(ctx, s);
    s = JS_ToCString(ctx, ret);
    JS_FreeValue(ctx, ret);
    assert(s);
    assert(!strcmp(s, "TypeError: from cfunc"));
    JS_FreeCString(ctx, s);

    ret = eval(ctx, "cfuncdata()");
    assert(JS_IsException(ret));
    ret = JS_GetException(ctx);
    assert(JS_IsError(ret));
    stack = JS_GetPropertyStr(ctx, ret, "stack");
    assert(JS_IsString(stack));
    s = JS_ToCString(ctx, stack);
    JS_FreeValue(ctx, stack);
    assert(s);
    assert(!strcmp(s, "    at cfuncdata (native)\n    at <eval> (<input>:1:1)\n"));
    JS_FreeCString(ctx, s);
    s = JS_ToCString(ctx, ret);
    JS_FreeValue(ctx, ret);
    assert(s);
    assert(!strcmp(s, "TypeError: from cfuncdata"));
    JS_FreeCString(ctx, s);

    ret = eval(ctx, "cclosure()");
    assert(JS_IsException(ret));
    ret = JS_GetException(ctx);
    assert(JS_IsError(ret));
    stack = JS_GetPropertyStr(ctx, ret, "stack");
    assert(JS_IsString(stack));
    s = JS_ToCString(ctx, stack);
    JS_FreeValue(ctx, stack);
    assert(s);
    assert(!strcmp(s, "    at cclosure (native)\n    at <eval> (<input>:1:1)\n"));
    JS_FreeCString(ctx, s);
    s = JS_ToCString(ctx, ret);
    JS_FreeValue(ctx, ret);
    assert(s);
    assert(!strcmp(s, "TypeError: from cclosure"));
    JS_FreeCString(ctx, s);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    assert(closure_finalized);
}

#define MAX_TIME 10

static int timeout_interrupt_handler(JSRuntime *rt, void *opaque)
{
    int *time = (int *)opaque;
    return (*time)++ > MAX_TIME;
}

static void sync_call(void)
{
    static const char code[] =
"(function() { \
    try { \
        while (true) {} \
    } catch (e) {} \
})();";

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    int time = 0;
    JS_SetInterruptHandler(rt, timeout_interrupt_handler, &time);
    JSValue ret = eval(ctx, code);
    assert(time > MAX_TIME);
    assert(JS_IsException(ret));
    JS_FreeValue(ctx, ret);
    assert(JS_HasException(ctx));
    JSValue e = JS_GetException(ctx);
    assert(JS_IsUncatchableError(e));
    JS_FreeValue(ctx, e);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void async_call(void)
{
    static const char code[] =
"(async function() { \
    const loop = async () => { \
        await Promise.resolve(); \
        while (true) {} \
    }; \
    await loop().catch(() => {}); \
})();";

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    int time = 0;
    JS_SetInterruptHandler(rt, timeout_interrupt_handler, &time);
    JSValue ret = eval(ctx, code);
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);
    assert(JS_IsJobPending(rt));
    int r = 0;
    while (JS_IsJobPending(rt)) {
        r = JS_ExecutePendingJob(rt, &ctx);
    }
    assert(time > MAX_TIME);
    assert(r == -1);
    assert(JS_HasException(ctx));
    JSValue e = JS_GetException(ctx);
    assert(JS_IsUncatchableError(e));
    JS_FreeValue(ctx, e);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void std_eval_interrupt_handler(void)
{
    static const char code[] =
        "import * as std from 'std'; std.evalScript('for(;;){}')";
    JSRuntime *rt = new_runtime();
    js_std_init_handlers(rt);
    JSContext *ctx = JS_NewContext(rt);
    js_init_module_std(ctx, "std");
    int time = 0;
    JS_SetInterruptHandler(rt, timeout_interrupt_handler, &time);
    JSValue ret =
        JS_Eval(ctx, code, strlen(code), "<input>", JS_EVAL_TYPE_MODULE);
    ret = js_std_await(ctx, ret);
    assert(time > MAX_TIME);
    assert(JS_IsException(ret));
    ret = JS_GetException(ctx);
    assert(JS_IsError(ret));
    // uncatchable "interrupted" exception is turned into a regular exception
    assert(!JS_IsUncatchableError(ret));
    const char *str = JS_ToCString(ctx, ret);
    assert(str != NULL);
    assert(strstr(str, "InternalError: interrupted"));
    JS_FreeCString(ctx, str);
    JS_FreeValue(ctx, ret);
    uintptr_t interrupt_handler = js_std_cmd(/*GetInterruptHandler*/5, rt);
    uintptr_t interrupt_opaque = js_std_cmd(/*GetInterruptOpaque*/6, rt);
    assert(interrupt_handler == (uintptr_t)timeout_interrupt_handler);
    assert(interrupt_opaque == (uintptr_t)&time);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static JSValue save_value(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv)
{
    assert(argc == 1);
    JSValue *p = (JSValue *)JS_GetContextOpaque(ctx);
    *p = JS_DupValue(ctx, argv[0]);
    return JS_UNDEFINED;
}

static void async_call_stack_overflow(void)
{
    static const char code[] =
"(async function() { \
    const f = () => f(); \
    try { \
        await Promise.resolve(); \
        f(); \
    } catch (e) { \
        save_value(e); \
    } \
})();";

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue value = JS_UNDEFINED;
    JS_SetContextOpaque(ctx, &value);
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "save_value", JS_NewCFunction(ctx, save_value, "save_value", 1));
    JS_FreeValue(ctx, global);
    JSValue ret = eval(ctx, code);
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);
    assert(JS_IsJobPending(rt));
    int r = 0;
    while (JS_IsJobPending(rt)) {
        r = JS_ExecutePendingJob(rt, &ctx);
    }
    assert(r == 1);
    assert(!JS_HasException(ctx));
    assert(JS_IsError(value)); // stack overflow should be caught
    JS_FreeValue(ctx, value);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

// https://github.com/quickjs-ng/quickjs/issues/914
static void raw_context_global_var(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContextRaw(rt);
    JS_AddIntrinsicEval(ctx);
    {
        JSValue ret = eval(ctx, "globalThis");
        assert(JS_IsException(ret));
        JS_FreeValue(ctx, ret);
    }
    {
        JSValue ret = eval(ctx, "var x = 42");
        assert(JS_IsUndefined(ret));
        JS_FreeValue(ctx, ret);
    }
    {
        JSValue ret = eval(ctx, "function f() {}");
        assert(JS_IsUndefined(ret));
        JS_FreeValue(ctx, ret);
    }
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void is_array(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    {
        JSValue ret = eval(ctx, "[]");
        assert(!JS_IsException(ret));
        assert(JS_IsArray(ret));
        JS_FreeValue(ctx, ret);
    }
    {
        JSValue ret = eval(ctx, "new Proxy([], {})");
        assert(!JS_IsException(ret));
        assert(!JS_IsArray(ret));
        assert(JS_IsProxy(ret));
        JSValue handler = JS_GetProxyHandler(ctx, ret);
        JSValue target = JS_GetProxyTarget(ctx, ret);
        assert(!JS_IsException(handler));
        assert(!JS_IsException(target));
        assert(!JS_IsProxy(handler));
        assert(!JS_IsProxy(target));
        assert(JS_IsObject(handler));
        assert(JS_IsArray(target));
        JS_FreeValue(ctx, handler);
        JS_FreeValue(ctx, target);
        JS_FreeValue(ctx, ret);
    }
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static int loader_calls;

static JSModuleDef *loader(JSContext *ctx, const char *name, void *opaque)
{
    loader_calls++;
    assert(!strcmp(name, "b"));
    static const char code[] = "export function f(x){}";
    JSValue ret = JS_Eval(ctx, code, strlen(code), "b",
                          JS_EVAL_TYPE_MODULE|JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(ret));
    JSModuleDef *m = JS_VALUE_GET_PTR(ret);
    assert(m);
    JS_FreeValue(ctx, ret);
    return m;
}

static void module_serde(void)
{
    JSRuntime *rt = new_runtime();
    //JS_SetDumpFlags(rt, JS_DUMP_MODULE_RESOLVE);
    JS_SetModuleLoaderFunc(rt, NULL, loader, NULL);
    JSContext *ctx = JS_NewContext(rt);
    static const char code[] = "import {f} from 'b'; f()";
    assert(loader_calls == 0);
    JSValue mod = JS_Eval(ctx, code, strlen(code), "a",
                          JS_EVAL_TYPE_MODULE|JS_EVAL_FLAG_COMPILE_ONLY);
    assert(loader_calls == 1);
    assert(!JS_IsException(mod));
    assert(JS_IsModule(mod));
    size_t len = 0;
    uint8_t *buf = JS_WriteObject(ctx, &len, mod,
                                  JS_WRITE_OBJ_BYTECODE|JS_WRITE_OBJ_REFERENCE);
    assert(buf);
    assert(len > 0);
    JS_FreeValue(ctx, mod);
    assert(loader_calls == 1);
    mod = JS_ReadObject(ctx, buf, len, JS_READ_OBJ_BYTECODE);
    js_free(ctx, buf);
    assert(loader_calls == 1); // 'b' is returned from cache
    assert(!JS_IsException(mod));
    JSValue ret = JS_EvalFunction(ctx, mod);
    assert(!JS_IsException(ret));
    assert(JS_IsPromise(ret));
    JSValue result = JS_PromiseResult(ctx, ret);
    assert(!JS_IsException(result));
    assert(JS_IsUndefined(result));
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, mod);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

struct rejection_counts {
    int reject_count;
    int handle_count;
};

static void rejection_counter(JSContext *ctx, JSValueConst promise,
                              JSValueConst reason, bool is_handled, void *opaque)
{
    struct rejection_counts *c = opaque;
    if (is_handled)
        c->handle_count++;
    else
        c->reject_count++;
}

// A synchronous module that throws at top level must surface exactly one unhandled rejection
static void module_unhandled_rejection(void)
{
    struct rejection_counts c = {0, 0};
    JSRuntime *rt = new_runtime();
    JS_SetHostPromiseRejectionTracker(rt, rejection_counter, &c);
    JSContext *ctx = JS_NewContext(rt);

    static const char code[] = "throw new Error('Nuke the entire site from orbit. It\\'s the only way to be sure.')";
    JSValue v = JS_Eval(ctx, code, strlen(code), "<m>", JS_EVAL_TYPE_MODULE);
    JS_FreeValue(ctx, v);

    JSContext *c1;
    while (JS_ExecutePendingJob(rt, &c1) > 0)
        ;

    // net unhandled rejections == (2 rejects - 1 handled)
    assert(c.reject_count == 2);
    assert(c.handle_count == 1);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void promise_mark_as_handled(void)
{
    struct rejection_counts c = {0, 0};
    JSRuntime *rt = new_runtime();
    JS_SetHostPromiseRejectionTracker(rt, rejection_counter, &c);
    JSContext *ctx = JS_NewContext(rt);
    JSContext *c1;

    // marking an already-rejected promise notifies the tracker exactly once
    static const char code[] = "Promise.reject('kaboom')";
    JSValue promise = JS_Eval(ctx, code, strlen(code), "<t>", JS_EVAL_TYPE_GLOBAL);
    assert(JS_IsPromise(promise));
    while (JS_ExecutePendingJob(rt, &c1) > 0)
        ;
    assert(c.reject_count == 1);
    assert(c.handle_count == 0);
    JS_PromiseMarkAsHandled(ctx, promise);
    assert(c.handle_count == 1);
    JS_PromiseMarkAsHandled(ctx, promise);
    assert(c.handle_count == 1);
    JS_FreeValue(ctx, promise);

    // marking a pending promise suppresses the report when it later rejects
    JSValue resolving_funcs[2];
    JSValue promise2 = JS_NewPromiseCapability(ctx, resolving_funcs);
    JS_PromiseMarkAsHandled(ctx, promise2);
    JSValue reason = JS_NewString(ctx, "unseen");
    JSValue ret = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1,
                          (JSValueConst *)&reason);
    while (JS_ExecutePendingJob(rt, &c1) > 0)
        ;
    assert(c.reject_count == 1);
    assert(c.handle_count == 1);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, reason);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise2);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void promise_then(void)
{
    const char *s;
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSContext *c1;
    JSValue got = JS_UNDEFINED;
    JS_SetContextOpaque(ctx, &got);

    // the result promise is intrinsic: neither an overridden then() nor
    // Symbol.species is consulted
    static const char code[] =
        "class P extends Promise {"
        "  static get [Symbol.species]() { throw new Error('species'); }"
        "  then() { throw new Error('then'); }"
        "}"
        "P.resolve('ok')";
    JSValue promise = eval(ctx, code);
    assert(JS_IsPromise(promise));
    JSValue on_fulfilled = JS_NewCFunction(ctx, save_value, "onFulfilled", 1);
    JSValue result = JS_PromiseThen(ctx, promise, on_fulfilled, JS_UNDEFINED);
    assert(JS_IsPromise(result));
    while (JS_ExecutePendingJob(rt, &c1) > 0)
        ;
    s = JS_ToCString(ctx, got);
    assert(s);
    assert(!strcmp(s, "ok"));
    JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, got);
    assert(JS_PromiseState(ctx, result) == JS_PROMISE_FULFILLED);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, on_fulfilled);
    JS_FreeValue(ctx, promise);

    // rejection handler receives the reason when the promise rejects later
    JSValue resolving_funcs[2];
    JSValue promise2 = JS_NewPromiseCapability(ctx, resolving_funcs);
    JSValue on_rejected = JS_NewCFunction(ctx, save_value, "onRejected", 1);
    JSValue result2 = JS_PromiseThen(ctx, promise2, JS_UNDEFINED, on_rejected);
    assert(JS_IsPromise(result2));
    got = JS_UNDEFINED;
    JSValue reason = JS_NewString(ctx, "boom");
    JSValue ret = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1,
                          (JSValueConst *)&reason);
    while (JS_ExecutePendingJob(rt, &c1) > 0)
        ;
    s = JS_ToCString(ctx, got);
    assert(s);
    assert(!strcmp(s, "boom"));
    JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, got);
    assert(JS_PromiseState(ctx, result2) == JS_PROMISE_FULFILLED);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, reason);
    JS_FreeValue(ctx, result2);
    JS_FreeValue(ctx, on_rejected);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise2);

    // a non-promise argument is a TypeError
    JSValue bad = JS_PromiseThen(ctx, JS_UNDEFINED, JS_UNDEFINED, JS_UNDEFINED);
    assert(JS_IsException(bad));
    JSValue exc = JS_GetException(ctx);
    JS_FreeValue(ctx, exc);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void runtime_cstring_free(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    // string -> cstring + JS_FreeCStringRT
    {
        JSValue ret = eval(ctx, "\"testStringPleaseIgnore\"");
        assert(JS_IsString(ret));
        const char *s = JS_ToCString(ctx, ret);
        assert(s);
        assert(strcmp(s, "testStringPleaseIgnore") == 0);
        JS_FreeCStringRT(rt, s);
        JS_FreeValue(ctx, ret);
    }
    // string -> cstring + JS_FreeCStringRT, destroying the source value first
    {
        JSValue ret = eval(ctx, "\"testStringPleaseIgnore\"");
        assert(JS_IsString(ret));
        const char *s = JS_ToCString(ctx, ret);
        assert(s);
        JS_FreeValue(ctx, ret);
        assert(strcmp(s, "testStringPleaseIgnore") == 0);
        JS_FreeCStringRT(rt, s);
    }
    // number -> cstring + JS_FreeCStringRT
    {
        JSValue ret = eval(ctx, "123987");
        assert(JS_IsNumber(ret));
        const char *s = JS_ToCString(ctx, ret);
        assert(s);
        assert(strcmp(s, "123987") == 0);
        JS_FreeCStringRT(rt, s);
        JS_FreeValue(ctx, ret);
    }
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void utf16_string(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    {
        JSValue v = JS_NewStringUTF16(ctx, NULL, 0);
        assert(!JS_IsException(v));
        const char *s = JS_ToCString(ctx, v);
        assert(s);
        assert(!strcmp(s, ""));
        JS_FreeCString(ctx, s);
        JS_FreeValue(ctx, v);
    }
    {
        JSValue v = JS_NewStringUTF16(ctx, (uint16_t[]){'o','k'}, 2);
        assert(!JS_IsException(v));
        const char *s = JS_ToCString(ctx, v);
        assert(s);
        assert(!strcmp(s, "ok"));
        JS_FreeCString(ctx, s);
        size_t n;
        const uint16_t *u = JS_ToCStringLenUTF16(ctx, &n, v);
        assert(u);
        assert(n == 2);
        assert(u[0] == 'o');
        assert(u[1] == 'k');
        JS_FreeCStringUTF16(ctx, u);
        JS_FreeValue(ctx, v);
    }
    {
        JSValue v = JS_NewStringUTF16(ctx, (uint16_t[]){0xD800}, 1);
        assert(!JS_IsException(v));
        const char *s = JS_ToCString(ctx, v);
        assert(s);
        // questionable but surrogates don't map to UTF-8 without WTF-8
        assert(!strcmp(s, "\xED\xA0\x80"));
        JS_FreeCString(ctx, s);
        size_t n;
        const uint16_t *u = JS_ToCStringLenUTF16(ctx, &n, v);
        assert(u);
        assert(n == 1);
        assert(u[0] == 0xD800);
        JS_FreeCStringUTF16(ctx, u);
        JS_FreeValue(ctx, v);
    }
    {
        JSValue v = JS_NewStringLen(ctx, "ok", 2); // ascii -> ucs
        assert(!JS_IsException(v));
        size_t n;
        const uint16_t *u = JS_ToCStringLenUTF16(ctx, &n, v);
        assert(u);
        assert(n == 2);
        assert(u[0] == 'o');
        assert(u[1] == 'k');
        JS_FreeCStringUTF16(ctx, u);
        JS_FreeValue(ctx, v);
    }
    {
        JSValue v = JS_NewStringUTF16(ctx, NULL, (size_t)INT_MAX + 1);
        assert(JS_IsException(v));
        JSValue e = JS_GetException(ctx);
        assert(JS_IsError(e));
        const char *s = JS_ToCString(ctx, e);
        assert(s);
        assert(strstr(s, "invalid string length") != NULL);
        JS_FreeCString(ctx, s);
        JS_FreeValue(ctx, e);
    }
    {
        // create wide char slice string, see JS_STRING_SLICE_LEN_MAX
        JSValue v = eval(ctx, "`\\uD800\\uDC00`.repeat(8192).slice(0, -1)");
        assert(!JS_IsException(v));
        size_t n;
        const uint16_t *u = JS_ToCStringLenUTF16(ctx, &n, v);
        assert(u);
        assert(n == 2*8192-1);
        assert(u[0] == 0xD800);
        assert(u[1] == 0xDC00);
        JS_FreeCStringUTF16(ctx, u);
        JS_FreeValue(ctx, v);
    }
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void weak_map_gc_check(void)
{
    static const char init_code[] =
"const map = new WeakMap(); \
function addItem() { \
    const k = { \
        text: 'a', \
    }; \
    map.set(k, {k}); \
}";
    static const char test_code[] = "addItem()";

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);

    JSValue ret = eval(ctx, init_code);
    assert(!JS_IsException(ret));

    JSValue ret_test = eval(ctx, test_code);
    assert(!JS_IsException(ret_test));
    JS_RunGC(rt);
    JSMemoryUsage memory_usage;
    JS_ComputeMemoryUsage(rt, &memory_usage);

    for (int i = 0; i < 3; i++) {
        JSValue ret_test2 = eval(ctx, test_code);
        assert(!JS_IsException(ret_test2));
        JS_RunGC(rt);
        JSMemoryUsage memory_usage2;
        JS_ComputeMemoryUsage(rt, &memory_usage2);

        assert(memory_usage.memory_used_count == memory_usage2.memory_used_count);
        assert(memory_usage.memory_used_size == memory_usage2.memory_used_size);
        JS_FreeValue(ctx, ret_test2);
    }

    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, ret_test);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

struct {
    int hook_type_call_count[4];
} promise_hook_state;

static void promise_hook_cb(JSContext *ctx, JSPromiseHookType type,
                            JSValueConst promise, JSValueConst parent_promise,
                            void *opaque)
{
    assert(type == JS_PROMISE_HOOK_INIT ||
           type == JS_PROMISE_HOOK_BEFORE ||
           type == JS_PROMISE_HOOK_AFTER ||
           type == JS_PROMISE_HOOK_RESOLVE);
    promise_hook_state.hook_type_call_count[type]++;
    assert(opaque == (void *)&promise_hook_state);
    if (!JS_IsUndefined(parent_promise)) {
        JSValue global_object = JS_GetGlobalObject(ctx);
        JS_SetPropertyStr(ctx, global_object, "actual",
                          JS_DupValue(ctx, parent_promise));
        JS_FreeValue(ctx, global_object);
    }
}

static void promise_hook(void)
{
    int *cc = promise_hook_state.hook_type_call_count;
    JSContext *unused;
    JSRuntime *rt = new_runtime();
    //JS_SetDumpFlags(rt, JS_DUMP_PROMISE);
    JS_SetPromiseHook(rt, promise_hook_cb, &promise_hook_state);
    JSContext *ctx = JS_NewContext(rt);
    JSValue global_object = JS_GetGlobalObject(ctx);
    {
        // empty module; creates an outer and inner module promise;
        // JS_Eval returns the outer promise
        JSValue ret = JS_Eval(ctx, "", 0, "<input>", JS_EVAL_TYPE_MODULE);
        assert(!JS_IsException(ret));
        assert(JS_IsPromise(ret));
        assert(JS_PROMISE_FULFILLED == JS_PromiseState(ctx, ret));
        JS_FreeValue(ctx, ret);
        assert(2 == cc[JS_PROMISE_HOOK_INIT]);
        assert(0 == cc[JS_PROMISE_HOOK_BEFORE]);
        assert(0 == cc[JS_PROMISE_HOOK_AFTER]);
        assert(2 == cc[JS_PROMISE_HOOK_RESOLVE]);
        assert(!JS_IsJobPending(rt));
    }
    memset(&promise_hook_state, 0, sizeof(promise_hook_state));
    {
        // module with unresolved promise; the outer and inner module promises
        // are resolved but not the user's promise
        static const char code[] = "new Promise(() => {})";
        JSValue ret = JS_Eval(ctx, code, strlen(code), "<input>", JS_EVAL_TYPE_MODULE);
        assert(!JS_IsException(ret));
        assert(JS_IsPromise(ret));
        assert(JS_PROMISE_FULFILLED == JS_PromiseState(ctx, ret)); // outer module promise
        JS_FreeValue(ctx, ret);
        assert(3 == cc[JS_PROMISE_HOOK_INIT]);
        assert(0 == cc[JS_PROMISE_HOOK_BEFORE]);
        assert(0 == cc[JS_PROMISE_HOOK_AFTER]);
        assert(2 == cc[JS_PROMISE_HOOK_RESOLVE]); // outer and inner module promise
        assert(!JS_IsJobPending(rt));
    }
    memset(&promise_hook_state, 0, sizeof(promise_hook_state));
    {
        // module with resolved promise
        static const char code[] = "new Promise((resolve,reject) => resolve())";
        JSValue ret = JS_Eval(ctx, code, strlen(code), "<input>", JS_EVAL_TYPE_MODULE);
        assert(!JS_IsException(ret));
        assert(JS_IsPromise(ret));
        assert(JS_PROMISE_FULFILLED == JS_PromiseState(ctx, ret)); // outer module promise
        JS_FreeValue(ctx, ret);
        assert(3 == cc[JS_PROMISE_HOOK_INIT]);
        assert(0 == cc[JS_PROMISE_HOOK_BEFORE]);
        assert(0 == cc[JS_PROMISE_HOOK_AFTER]);
        assert(3 == cc[JS_PROMISE_HOOK_RESOLVE]);
        assert(!JS_IsJobPending(rt));
    }
    memset(&promise_hook_state, 0, sizeof(promise_hook_state));
    {
        // module with rejected promise
        static const char code[] = "new Promise((resolve,reject) => reject())";
        JSValue ret = JS_Eval(ctx, code, strlen(code), "<input>", JS_EVAL_TYPE_MODULE);
        assert(!JS_IsException(ret));
        assert(JS_IsPromise(ret));
        assert(JS_PROMISE_FULFILLED == JS_PromiseState(ctx, ret)); // outer module promise
        JS_FreeValue(ctx, ret);
        assert(3 == cc[JS_PROMISE_HOOK_INIT]);
        assert(0 == cc[JS_PROMISE_HOOK_BEFORE]);
        assert(0 == cc[JS_PROMISE_HOOK_AFTER]);
        assert(2 == cc[JS_PROMISE_HOOK_RESOLVE]);
        assert(!JS_IsJobPending(rt));
    }
    memset(&promise_hook_state, 0, sizeof(promise_hook_state));
    {
        // module with promise chain
        static const char code[] =
            "globalThis.count = 0;"
            "globalThis.actual = undefined;" // set by promise_hook_cb
            "globalThis.expected = new Promise(resolve => resolve());"
            "expected.then(_ => count++)";
        JSValue ret = JS_Eval(ctx, code, strlen(code), "<input>", JS_EVAL_TYPE_MODULE);
        assert(!JS_IsException(ret));
        assert(JS_IsPromise(ret));
        assert(JS_PROMISE_FULFILLED == JS_PromiseState(ctx, ret)); // outer module promise
        JS_FreeValue(ctx, ret);
        assert(4 == cc[JS_PROMISE_HOOK_INIT]);
        assert(0 == cc[JS_PROMISE_HOOK_BEFORE]);
        assert(0 == cc[JS_PROMISE_HOOK_AFTER]);
        assert(3 == cc[JS_PROMISE_HOOK_RESOLVE]);
        JSValue v = JS_GetPropertyStr(ctx, global_object, "count");
        assert(!JS_IsException(v));
        int32_t count;
        assert(0 == JS_ToInt32(ctx, &count, v));
        assert(0 == count);
        JS_FreeValue(ctx, v);
        assert(JS_IsJobPending(rt));
        assert(1 == JS_ExecutePendingJob(rt, &unused));
        assert(!JS_HasException(ctx));
        assert(4 == cc[JS_PROMISE_HOOK_INIT]);
        assert(0 == cc[JS_PROMISE_HOOK_BEFORE]);
        assert(0 == cc[JS_PROMISE_HOOK_AFTER]);
        assert(4 == cc[JS_PROMISE_HOOK_RESOLVE]);
        assert(!JS_IsJobPending(rt));
        v = JS_GetPropertyStr(ctx, global_object, "count");
        assert(!JS_IsException(v));
        assert(0 == JS_ToInt32(ctx, &count, v));
        assert(1 == count);
        JS_FreeValue(ctx, v);
        JSValue actual = JS_GetPropertyStr(ctx, global_object, "actual");
        JSValue expected = JS_GetPropertyStr(ctx, global_object, "expected");
        assert(!JS_IsException(actual));
        assert(!JS_IsException(expected));
        assert(JS_IsSameValue(ctx, actual, expected));
        JS_FreeValue(ctx, actual);
        JS_FreeValue(ctx, expected);
    }
    memset(&promise_hook_state, 0, sizeof(promise_hook_state));
    {
        // module with thenable; fires before and after hooks
        static const char code[] =
            "new Promise(resolve => resolve({then(resolve){ resolve() }}))";
        JSValue ret = JS_Eval(ctx, code, strlen(code), "<input>", JS_EVAL_TYPE_MODULE);
        assert(!JS_IsException(ret));
        assert(JS_IsPromise(ret));
        assert(JS_PROMISE_FULFILLED == JS_PromiseState(ctx, ret)); // outer module promise
        JS_FreeValue(ctx, ret);
        assert(3 == cc[JS_PROMISE_HOOK_INIT]);
        assert(0 == cc[JS_PROMISE_HOOK_BEFORE]);
        assert(0 == cc[JS_PROMISE_HOOK_AFTER]);
        assert(2 == cc[JS_PROMISE_HOOK_RESOLVE]);
        assert(JS_IsJobPending(rt));
        assert(1 == JS_ExecutePendingJob(rt, &unused));
        assert(!JS_HasException(ctx));
        assert(3 == cc[JS_PROMISE_HOOK_INIT]);
        assert(1 == cc[JS_PROMISE_HOOK_BEFORE]);
        assert(1 == cc[JS_PROMISE_HOOK_AFTER]);
        assert(3 == cc[JS_PROMISE_HOOK_RESOLVE]);
        assert(!JS_IsJobPending(rt));
    }
    JS_FreeValue(ctx, global_object);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void dump_memory_usage(void)
{
    JSMemoryUsage stats;

    JSRuntime *rt = NULL;
    JSContext *ctx = NULL;

    rt = new_runtime();
    ctx = JS_NewContext(rt);

    //JS_SetDumpFlags(rt, JS_DUMP_PROMISE);

    static const char code[] =
    "globalThis.count = 0;"
    "globalThis.actual = undefined;" // set by promise_hook_cb
    "globalThis.expected = new Promise(resolve => resolve());"
    "expected.then(_ => count++)";

    JSValue evalVal = JS_Eval(ctx, code, strlen(code), "<input>", 0);
    JS_FreeValue(ctx, evalVal);

    FILE *temp = tmpfile();
    assert(temp != NULL);
    JS_ComputeMemoryUsage(rt, &stats);
    JS_DumpMemoryUsage(temp, &stats, rt);
    // JS_DumpMemoryUsage(stdout, &stats, rt);
    fclose(temp);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void new_errors(void)
{
    typedef struct {
        const char name[16];
        JSValue (*func)(JSContext *, const char *, ...);
    } Entry;
    static const Entry entries[] = {
        {"Error",           JS_NewPlainError},
        {"InternalError",   JS_NewInternalError},
        {"RangeError",      JS_NewRangeError},
        {"ReferenceError",  JS_NewReferenceError},
        {"SyntaxError",     JS_NewSyntaxError},
        {"TypeError",       JS_NewTypeError},
    };
    const Entry *e;

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    for (e = entries; e < endof(entries); e++) {
        JSValue obj = (*e->func)(ctx, "the %s", "needle");
        assert(!JS_IsException(obj));
        assert(JS_IsObject(obj));
        assert(JS_IsError(obj));
        const char *haystack = JS_ToCString(ctx, obj);
        char needle[256];
        snprintf(needle, sizeof(needle), "%s: the needle", e->name);
        assert(strstr(haystack, needle));
        JS_FreeCString(ctx, haystack);
        JSValue stack = JS_GetPropertyStr(ctx, obj, "stack");
        assert(!JS_IsException(stack));
        assert(JS_IsString(stack));
        JS_FreeValue(ctx, stack);
        JS_FreeValue(ctx, obj);
    }
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

// JS_NewContext() already installs DOMException by way of
// JS_AddIntrinsicAToB(), so a host that also calls JS_AddIntrinsicDOMException()
// explicitly installs it twice. The second install must release the prototype
// the first one left in ctx->class_proto[] instead of overwriting the slot;
// new_runtime() aborts at JS_FreeRuntime() if it does not.
static void dom_exception_added_twice(void)
{
    JSValue ret;
    const char *s;
    int i;

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);

    // the implicit install left a working DOMException behind
    ret = eval(ctx, "DOMException.prototype === "
                    "Object.getPrototypeOf(new DOMException)");
    assert(JS_ToBool(ctx, ret) == true);
    JS_FreeValue(ctx, ret);

    // installing it again, repeatedly, must not leak the previous prototype
    for (i = 0; i < 3; i++)
        assert(JS_AddIntrinsicDOMException(ctx) == 0);

    // and the class prototype still matches the global constructor, i.e. the
    // slot tracks the newest install rather than a freed or stale object
    ret = eval(ctx, "DOMException.prototype === "
                    "Object.getPrototypeOf(new DOMException)");
    assert(JS_ToBool(ctx, ret) == true);
    JS_FreeValue(ctx, ret);

    ret = eval(ctx, "new DOMException('m', 'InvalidCharacterError').name");
    assert(!JS_IsException(ret));
    s = JS_ToCString(ctx, ret);
    assert(s);
    assert(!strcmp(s, "InvalidCharacterError"));
    JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, ret);

    // the internally thrown DOMExceptions pick up the current prototype too
    ret = eval(ctx, "try { atob('a') } catch (e) { "
                    "  e instanceof DOMException && e.name } ");
    assert(!JS_IsException(ret));
    s = JS_ToCString(ctx, ret);
    assert(s);
    assert(!strcmp(s, "InvalidCharacterError"));
    JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, ret);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void backtrace_oom_callsite_array(void)
{
    static const char setup_code[] =
        "Error.prepareStackTrace = (e, frames) => frames;\n"
        "globalThis.f = () => new Error();\n";
    JSValue global_object, func, ret;
    JSMemoryUsage stats;
    uint32_t headroom;
    JSRuntime *rt;
    JSContext *ctx;

    rt = new_runtime();
    ctx = JS_NewContext(rt);
    global_object = JS_GetGlobalObject(ctx);

    ret = eval(ctx, setup_code);
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);

    func = JS_GetPropertyStr(ctx, global_object, "f");
    assert(JS_IsFunction(ctx, func));

    for (headroom = 0; headroom < 2048; headroom++) {
        JS_ComputeMemoryUsage(rt, &stats);
        JS_SetMemoryLimit(rt, (size_t)stats.malloc_size + headroom);
        ret = JS_Call(ctx, func, JS_UNDEFINED, 0, NULL);
        JS_SetMemoryLimit(rt, 0);
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, JS_GetException(ctx));
    }

    JS_FreeValue(ctx, func);
    JS_FreeValue(ctx, global_object);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void large_allocation_accounting(void)
{
    static const size_t block_size = 1024 * 1024;
    JSMemoryUsage before, after;
    JSValue ret, exception;
    JSRuntime *rt;
    JSContext *ctx;
    const char *str;

    rt = new_runtime();
    ctx = JS_NewContext(rt);

    JS_ComputeMemoryUsage(rt, &before);
    ret = eval(ctx, "globalThis.a = new Uint8Array(1024 * 1024)");
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);
    JS_ComputeMemoryUsage(rt, &after);
    assert(after.malloc_size - before.malloc_size >= (int64_t)block_size);

    JS_ComputeMemoryUsage(rt, &before);
    JS_SetMemoryLimit(rt, (size_t)before.malloc_size + 4 * block_size);
    ret = eval(ctx, "globalThis.a = [];\n"
                    "for (let i = 0; i < 64; i++)\n"
                    "    a.push(new Uint8Array(1024 * 1024));");
    assert(JS_IsException(ret));
    JS_SetMemoryLimit(rt, 0);
    JS_ComputeMemoryUsage(rt, &after);
    assert(after.malloc_size - before.malloc_size < (int64_t)(8 * block_size));

    exception = JS_GetException(ctx);
    assert(JS_IsError(exception));
    str = JS_ToCString(ctx, exception);
    assert(str);
    assert(!strcmp(str, "InternalError: out of memory"));
    JS_FreeCString(ctx, str);
    JS_FreeValue(ctx, exception);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void backtrace_oom_current_exception(void)
{
    static const char setup_code[] =
        "globalThis.f = function() { missing; };\n"
        "Object.defineProperty(f, 'name', { value: 'x'.repeat(2 * 1024 * 1024) });";
    JSMemoryUsage stats;
    JSValue ret, exception;
    JSRuntime *rt;
    JSContext *ctx;

    rt = new_runtime();
    ctx = JS_NewContext(rt);

    ret = eval(ctx, setup_code);
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);

    JS_ComputeMemoryUsage(rt, &stats);
    JS_SetMemoryLimit(rt, (size_t)stats.malloc_size + 128 * 1024);

    ret = eval(ctx, "f()");
    assert(JS_IsException(ret));
    assert(JS_HasException(ctx));
    exception = JS_GetException(ctx);
    JS_FreeValue(ctx, exception);
    JS_SetMemoryLimit(rt, 0);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void proxy_own_keys_huge_length(void)
{
    static const char setup_code[] =
        "globalThis.p = new Proxy({}, {\n"
        "    ownKeys() { return { length: 0x20000000 }; },\n"
        "});";
    JSMemoryUsage stats;
    JSValue ret, exception;
    JSRuntime *rt;
    JSContext *ctx;
    const char *str;

    rt = new_runtime();
    ctx = JS_NewContext(rt);

    ret = eval(ctx, setup_code);
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);

    JS_ComputeMemoryUsage(rt, &stats);
    JS_SetMemoryLimit(rt, (size_t)stats.malloc_size + 128 * 1024);

    ret = eval(ctx, "Object.keys(p)");
    assert(JS_IsException(ret));
    JS_FreeValue(ctx, ret);
    exception = JS_GetException(ctx);
    str = JS_ToCString(ctx, exception);
    assert(str);
    /* the trap result has no index 0, so the first key must be rejected */
    assert(!strcmp(str, "TypeError: proxy: properties must be strings or symbols"));
    JS_FreeCString(ctx, str);
    JS_FreeValue(ctx, exception);
    JS_SetMemoryLimit(rt, 0);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static int gop_get_own_property(JSContext *ctx, JSPropertyDescriptor *desc,
                                JSValueConst obj, JSAtom prop)
{
    const char *name;
    int found;

    found = 0;
    name = JS_AtomToCString(ctx, prop);
    if (!name)
        return -1;
    if (!strcmp(name, "answer")) {
        found = 1;
        *desc = (JSPropertyDescriptor){
            .value = JS_NewInt32(ctx, 42),
            .flags = JS_PROP_C_W_E | JS_PROP_HAS_VALUE,
        };
    }
    JS_FreeCString(ctx, name);
    return found;
}

static void global_object_prototype(void)
{
    static const char code[] = "answer";
    JSValue global_object, proto, ret;
    JSClassID class_id;
    JSRuntime *rt;
    JSContext *ctx;
    int32_t answer;
    int res;

    {
        rt = new_runtime();
        ctx = JS_NewContext(rt);
        proto = JS_NewObject(ctx);
        assert(JS_IsObject(proto));
        JSCFunctionListEntry prop = JS_PROP_INT32_DEF("answer", 42, JS_PROP_C_W_E);
        JS_SetPropertyFunctionList(ctx, proto, &prop, 1);
        global_object = JS_GetGlobalObject(ctx);
        res = JS_SetPrototype(ctx, global_object, proto);
        assert(res == true);
        JS_FreeValue(ctx, global_object);
        JS_FreeValue(ctx, proto);
        ret = eval(ctx, code);
        assert(!JS_IsException(ret));
        assert(JS_IsNumber(ret));
        res = JS_ToInt32(ctx, &answer, ret);
        assert(res == 0);
        assert(answer == 42);
        JS_FreeValue(ctx, ret);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
    }
    {
        JSClassExoticMethods exotic = (JSClassExoticMethods){
            .get_own_property = gop_get_own_property,
        };
        JSClassDef def = (JSClassDef){
            .class_name = "Global Object",
            .exotic = &exotic,
        };
        rt = new_runtime();
        class_id = 0;
        JS_NewClassID(rt, &class_id);
        res = JS_NewClass(rt, class_id, &def);
        assert(res == 0);
        ctx = JS_NewContext(rt);
        proto = JS_NewObjectClass(ctx, class_id);
        assert(JS_IsObject(proto));
        global_object = JS_GetGlobalObject(ctx);
        res = JS_SetPrototype(ctx, global_object, proto);
        assert(res == true);
        JS_FreeValue(ctx, global_object);
        JS_FreeValue(ctx, proto);
        ret = eval(ctx, code);
        assert(!JS_IsException(ret));
        assert(JS_IsNumber(ret));
        res = JS_ToInt32(ctx, &answer, ret);
        assert(res == 0);
        assert(answer == 42);
        JS_FreeValue(ctx, ret);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
    }
}

// https://github.com/quickjs-ng/quickjs/issues/1178
static void slice_string_tocstring(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue ret = eval(ctx, "'.'.repeat(16384).slice(1, -1)");
    assert(!JS_IsException(ret));
    assert(JS_IsString(ret));
    const char *str = JS_ToCString(ctx, ret);
    assert(strlen(str) == 16382);
    JS_FreeCString(ctx, str);
    JS_FreeValue(ctx, ret);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void immutable_array_buffer(void)
{
    JSValue obj, ret;
    bool immutable;
    char buf[96];
    int i, v;

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    for (i = 0; i < 2; i++) {
        obj = JS_NewObject(ctx);
        immutable = (i == 0);
        assert(-1 == JS_IsImmutableArrayBuffer(JS_NULL));
        assert(-1 == JS_IsImmutableArrayBuffer(JS_UNDEFINED));
        assert(-1 == JS_IsImmutableArrayBuffer(obj));
        assert(-1 == JS_SetImmutableArrayBuffer(JS_NULL, immutable));
        assert(-1 == JS_SetImmutableArrayBuffer(JS_UNDEFINED, immutable));
        assert(-1 == JS_SetImmutableArrayBuffer(obj, immutable));
        JS_FreeValue(ctx, obj);
    }
    obj = eval(ctx, "globalThis.ab = new ArrayBuffer(1)");
    assert(!JS_IsException(obj));
    assert(JS_IsArrayBuffer(obj));
    assert(!JS_IsImmutableArrayBuffer(obj));
    for (i = 1; i <= 3; i++) {
        immutable = (i == 2);
        if (i > 1)
            JS_SetImmutableArrayBuffer(obj, immutable);
        assert(immutable == JS_IsImmutableArrayBuffer(obj));
        snprintf(buf, sizeof(buf),
                 "var ta = new Uint8Array(ab); ta[0] = %d; ta[0]", i);
        ret = eval(ctx, buf);
        assert(!JS_IsException(ret));
        assert(JS_IsNumber(ret));
        assert(0 == JS_ToInt32(ctx, &v, ret));
        JS_FreeValue(ctx, ret);
        if (immutable) {
            assert(v != i);
        } else {
            assert(v == i);
        }
    }
    JS_FreeValue(ctx, obj);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void *sab_test_alloc(void *opaque, size_t size)
{
    return malloc(size);
}

static void sab_test_free(void *opaque, void *ptr)
{
    free(ptr);
}

static void shared_array_buffer_growth(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue ret, exception;

    ret = eval(ctx, "new SharedArrayBuffer(16)");
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);

    ret = eval(ctx,
               "const sab = new SharedArrayBuffer(16, { maxByteLength: 16 });"
               "sab.grow(16);"
               "sab.byteLength === 16 && sab.maxByteLength === 16");
    assert(!JS_IsException(ret));
    assert(JS_IsBool(ret));
    assert(JS_VALUE_GET_BOOL(ret));
    JS_FreeValue(ctx, ret);

    ret = eval(ctx, "new SharedArrayBuffer(16, { maxByteLength: 16384 })");
    assert(JS_IsException(ret));
    assert(JS_HasException(ctx));
    exception = JS_GetException(ctx);
    assert(JS_IsError(exception));
    JS_FreeValue(ctx, exception);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    JSSharedArrayBufferFunctions funcs = {
        .sab_alloc = sab_test_alloc,
        .sab_free = sab_test_free,
        .sab_dup = NULL,
        .sab_opaque = NULL,
    };

    rt = new_runtime();
    JS_SetSharedArrayBufferFunctions(rt, &funcs);
    ctx = JS_NewContext(rt);
    ret = eval(ctx,
               "const sab = new SharedArrayBuffer(16, { maxByteLength: 16384 });"
               "const u8 = new Uint8Array(sab);"
               "sab.grow(16384);"
               "u8[1024] === 0 && u8.byteLength === 16384");
    assert(!JS_IsException(ret));
    assert(JS_IsBool(ret));
    assert(JS_VALUE_GET_BOOL(ret));
    JS_FreeValue(ctx, ret);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void get_uint8array(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue val;
    uint8_t *p;
    size_t size;
    uint8_t buf[3] = { 1, 2, 3 };

    val = eval(ctx, "new Uint8Array(0)");
    assert(!JS_IsException(val));
    p = JS_GetUint8Array(ctx, &size, val);
    assert(p != NULL);
    assert(size == 0);
    JS_FreeValue(ctx, val);

    val = JS_NewUint8Array(ctx, NULL, 0, NULL, NULL, false);
    assert(!JS_IsException(val));
    p = JS_GetUint8Array(ctx, &size, val);
    assert(p != NULL);
    assert(size == 0);
    JS_FreeValue(ctx, val);

    val = JS_NewUint8ArrayCopy(ctx, NULL, 0);
    assert(!JS_IsException(val));
    p = JS_GetUint8Array(ctx, &size, val);
    assert(p != NULL);
    assert(size == 0);
    JS_FreeValue(ctx, val);

    val = JS_NewUint8ArrayCopy(ctx, buf, sizeof(buf));
    assert(!JS_IsException(val));
    p = JS_GetUint8Array(ctx, &size, val);
    assert(p != NULL);
    assert(size == 3);
    assert(p[0] == 1 && p[1] == 2 && p[2] == 3);
    JS_FreeValue(ctx, val);

    val = eval(ctx, "new Uint8Array([4, 5, 6])");
    assert(!JS_IsException(val));
    p = JS_GetUint8Array(ctx, &size, val);
    assert(p != NULL);
    assert(size == 3);
    assert(p[0] == 4 && p[1] == 5 && p[2] == 6);
    JS_FreeValue(ctx, val);

    val = eval(ctx, "new Int32Array(4)");
    assert(!JS_IsException(val));
    p = JS_GetUint8Array(ctx, &size, val);
    assert(p == NULL);
    assert(size == 0);
    JS_FreeValue(ctx, val);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void new_symbol(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue sym, ret;

    /* Local symbol with NULL description -> Symbol() */
    sym = JS_NewSymbol(ctx, NULL, false);
    assert(!JS_IsException(sym));
    assert(JS_IsSymbol(sym));
    JS_SetPropertyStr(ctx, global, "sym_local_null", sym);

    ret = eval(ctx, "typeof sym_local_null === 'symbol' && sym_local_null.description === undefined && Symbol.keyFor(sym_local_null) === undefined");
    assert(JS_IsBool(ret));
    assert(JS_VALUE_GET_BOOL(ret));
    JS_FreeValue(ctx, ret);

    /* Global symbol with NULL description -> Symbol.for() -> Symbol.for('undefined') */
    sym = JS_NewSymbol(ctx, NULL, true);
    assert(!JS_IsException(sym));
    assert(JS_IsSymbol(sym));
    JS_SetPropertyStr(ctx, global, "sym_global_null", sym);

    ret = eval(ctx, "typeof sym_global_null === 'symbol' && sym_global_null.description === 'undefined' && Symbol.keyFor(sym_global_null) === 'undefined'");
    assert(JS_IsBool(ret));
    assert(JS_VALUE_GET_BOOL(ret));
    JS_FreeValue(ctx, ret);

    /* Local symbol with description -> Symbol('test_local') */
    sym = JS_NewSymbol(ctx, "test_local", false);
    assert(!JS_IsException(sym));
    assert(JS_IsSymbol(sym));
    JS_SetPropertyStr(ctx, global, "sym_local_str", sym);

    ret = eval(ctx, "sym_local_str.description === 'test_local' && Symbol.keyFor(sym_local_str) === undefined");
    assert(JS_IsBool(ret));
    assert(JS_VALUE_GET_BOOL(ret));
    JS_FreeValue(ctx, ret);

    /* Global symbol with description -> Symbol.for('test_global') */
    sym = JS_NewSymbol(ctx, "test_global", true);
    assert(!JS_IsException(sym));
    assert(JS_IsSymbol(sym));
    JS_SetPropertyStr(ctx, global, "sym_global_str", sym);

    ret = eval(ctx, "sym_global_str.description === 'test_global' && Symbol.keyFor(sym_global_str) === 'test_global'");
    assert(JS_IsBool(ret));
    assert(JS_VALUE_GET_BOOL(ret));
    JS_FreeValue(ctx, ret);

    JS_FreeValue(ctx, global);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void bulk_free_macros(void) {
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);

    JSValue val0 = JS_NewObject(ctx);
    JSValue val1 = JS_NewArray(ctx);
    JSValue val2 = JS_NewDate(ctx, 0.0);
    
    JSMemoryUsage mem_usage;
    JS_ComputeMemoryUsage(rt, &mem_usage);
    int obj_count = mem_usage.obj_count;

    JS_FreeValues(ctx, val0, val1);
    JS_FreeValuesRT(rt, val2);

    // silly atoms to ensure qjs doesn't find built-ins that match
    JSAtom atom0 = JS_NewAtom(ctx, "ALL!!");
    JSAtom atom1 = JS_NewAtom(ctx, "YOUR!!");
    JSAtom atom2 = JS_NewAtom(ctx, "ATOMS!!");
    JSAtom atom3 = JS_NewAtom(ctx, "ARE!!");
    JSAtom atom4 = JS_NewAtom(ctx, "BELONG!!");
    JSAtom atom5 = JS_NewAtom(ctx, "TO US!!");

    JS_ComputeMemoryUsage(rt, &mem_usage);
    assert((obj_count - 3) == mem_usage.obj_count);
    int atom_count = mem_usage.atom_count;

    JS_FreeAtoms(ctx, atom1, atom3, atom4);
    JS_FreeAtomsRT(rt, atom0, atom2, atom5);

    JS_ComputeMemoryUsage(rt, &mem_usage);
    assert((atom_count - 6) == mem_usage.atom_count);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static int detach_free_count;

static void *detach_realloc_func(JSRuntime *rt, void *opaque, void *ptr,
                                 size_t size)
{
    if (size == 0) {
        detach_free_count++;
        free(ptr);
        return NULL;
    }
    return realloc(ptr, size);
}

static void detach_array_buffer_free_once(void)
{
    JSValue obj;
    uint8_t *buf;

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);

    detach_free_count = 0;
    buf = malloc(8);
    obj = JS_NewArrayBuffer(ctx, buf, 8, /*max_len*/0, detach_realloc_func,
                            NULL, false);
    assert(JS_IsArrayBuffer(obj));

    /* detaching releases the backing store exactly once */
    JS_DetachArrayBuffer(ctx, obj);
    assert(detach_free_count == 1);

    /* finalizing the detached buffer must not release it a second time */
    JS_FreeValue(ctx, obj);
    assert(detach_free_count == 1);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void *realloc_array_buffer_data(JSRuntime *rt, void *opaque, void *ptr,
                                       size_t size)
{
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, size);
}

static void typed_array_sort_index_overflow(void)
{
    const size_t len = 0x40000010; /* 2**30 + 16 */
    JSMemoryUsage stats;
    JSValue global, buffer, ret;
    JSRuntime *rt;
    JSContext *ctx;
    uint8_t *buf;

    /* the data is never read, but keep it real memory so a regression
       corrupts the heap in a way ASan can see rather than segfaulting */
    buf = calloc(1, len);
    rt = new_runtime();
    ctx = JS_NewContext(rt);

    buffer = JS_NewArrayBuffer(ctx, buf, len, /*max_len*/0,
                               realloc_array_buffer_data, NULL, false);
    assert(JS_IsArrayBuffer(buffer));
    global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "buf", buffer);
    JS_FreeValue(ctx, global);

    /* Cap memory so that the correctly computed 4 GB index array also
       fails to allocate on 64 bit platforms; the undersized allocation a
       32 bit platform computes fits well within the cap. */
    JS_ComputeMemoryUsage(rt, &stats);
    JS_SetMemoryLimit(rt, (size_t)stats.malloc_size + 1024 * 1024);

    ret = eval(ctx, "new Uint8Array(buf).sort((a, b) => a - b)");
    assert(JS_IsException(ret));
    assert(JS_HasException(ctx));
    JS_FreeValue(ctx, ret);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static int transfer_free_count;
static int transfer_realloc_count;

static void *transfer_realloc_func(JSRuntime *rt, void *opaque, void *ptr,
                                   size_t size)
{
    if (size == 0) {
        transfer_free_count++;
        free(ptr);
        return NULL;
    }
    transfer_realloc_count++;
    return realloc(ptr, size);
}

/* a realloc function that can only free, never resize */
static void *failing_realloc_func(JSRuntime *rt, void *opaque, void *ptr,
                                  size_t size)
{
    if (size == 0) {
        transfer_free_count++;
        free(ptr);
    }
    return NULL;
}

static JSValue make_external_ab(JSContext *ctx, uint8_t **pbuf, size_t len,
                                size_t max_len,
                                JSReallocArrayBufferDataFunc *realloc_func)
{
    uint8_t *buf = malloc(max_len > len ? max_len : len);
    memset(buf, 0xAB, len);
    JSValue obj = JS_NewArrayBuffer(ctx, buf, len, max_len, realloc_func, NULL,
                                    /*is_shared*/false);
    assert(JS_IsArrayBuffer(obj));
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "ab", JS_DupValue(ctx, obj));
    JS_FreeValue(ctx, global);
    *pbuf = buf;
    return obj;
}

static void transfer_external_array_buffer(void)
{
    JSValue obj, ret, exc, det;
    const char *s;
    uint8_t *buf, *p;
    size_t i, size;

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);

    // same-length transfer succeeds and reuses the backing store; each variant
    // detaches the source, so use a fresh buffer per iteration
    {
        static const char *const exprs[] = {
            "ab.transfer()", "ab.transfer(8)", "ab.transferToFixedLength()",
            "ab.transferToFixedLength(8)", "ab.transferToImmutable()",
        };
        for (i = 0; i < countof(exprs); i++) {
            transfer_free_count = 0;
            transfer_realloc_count = 0;
            obj = make_external_ab(ctx, &buf, 8, /*max_len*/0,
                                   transfer_realloc_func);

            ret = eval(ctx, exprs[i]);
            assert(!JS_IsException(ret));
            assert(JS_IsArrayBuffer(ret));
            assert(transfer_realloc_count == 0);
            p = JS_GetArrayBuffer(ctx, &size, ret);
            assert(p == buf);
            assert(size == 8);
            assert(p[0] == 0xAB && p[7] == 0xAB);
            det = eval(ctx, "ab.detached");
            assert(JS_IsBool(det));
            assert(JS_VALUE_GET_BOOL(det));
            JS_FreeValue(ctx, det);
            assert(transfer_free_count == 0);
            JS_FreeValue(ctx, ret);
            assert(transfer_free_count == 1);

            ret = eval(ctx, "globalThis.ab = undefined");
            JS_FreeValue(ctx, ret);
            JS_FreeValue(ctx, obj);
            assert(transfer_free_count == 1);
        }
    }

    // a length-changing transfer resizes the external store through the
    // realloc function and hands it over to the new ArrayBuffer
    {
        static const struct {
            const char *code;
            size_t len;
        } cases[] = {
            { "ab.transfer(4)", 4 },
            { "ab.transfer(16)", 16 },
            { "ab.transferToFixedLength(4)", 4 },
            { "ab.transferToImmutable(16)", 16 },
        };
        for (i = 0; i < countof(cases); i++) {
            transfer_free_count = 0;
            transfer_realloc_count = 0;
            obj = make_external_ab(ctx, &buf, 8, /*max_len*/0,
                                   transfer_realloc_func);

            ret = eval(ctx, cases[i].code);
            assert(!JS_IsException(ret));
            assert(JS_IsArrayBuffer(ret));
            assert(transfer_realloc_count == 1);
            assert(transfer_free_count == 0);
            p = JS_GetArrayBuffer(ctx, &size, ret);
            assert(size == cases[i].len);
            assert(p[0] == 0xAB && p[3] == 0xAB);
            if (cases[i].len > 8)
                assert(p[8] == 0 && p[15] == 0); // the grown tail is zeroed
            det = eval(ctx, "ab.detached");
            assert(JS_IsBool(det));
            assert(JS_VALUE_GET_BOOL(det));
            JS_FreeValue(ctx, det);

            // the source handed the store over; releasing the new AB frees it
            JS_FreeValue(ctx, ret);
            assert(transfer_free_count == 1);

            ret = eval(ctx, "globalThis.ab = undefined");
            JS_FreeValue(ctx, ret);
            JS_FreeValue(ctx, obj);
            assert(transfer_free_count == 1);
        }
    }

    // a transfer that fails to realloc throws and leaves the source alone
    {
        transfer_free_count = 0;
        obj = make_external_ab(ctx, &buf, 8, /*max_len*/0,
                               failing_realloc_func);

        ret = eval(ctx, "ab.transfer(16)");
        assert(JS_IsException(ret));
        JS_FreeValue(ctx, ret);
        exc = JS_GetException(ctx);
        assert(JS_IsError(exc));
        s = JS_ToCString(ctx, exc);
        assert(s);
        assert(strstr(s, "out of memory"));
        JS_FreeCString(ctx, s);
        JS_FreeValue(ctx, exc);

        det = eval(ctx, "ab.detached");
        assert(JS_IsBool(det));
        assert(!JS_VALUE_GET_BOOL(det));
        JS_FreeValue(ctx, det);
        p = JS_GetArrayBuffer(ctx, &size, obj);
        assert(p == buf && size == 8 && p[0] == 0xAB);
        assert(transfer_free_count == 0);

        ret = eval(ctx, "globalThis.ab = undefined");
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, obj);
        assert(transfer_free_count == 1);
    }

    // transfer(0) changes the length but doesn't realloc: it detaches the
    // source (freeing the external store) and yields a fresh empty AB
    {
        transfer_free_count = 0;
        transfer_realloc_count = 0;
        obj = make_external_ab(ctx, &buf, 8, /*max_len*/0,
                               transfer_realloc_func);

        ret = eval(ctx, "ab.transfer(0)");
        assert(!JS_IsException(ret));
        assert(JS_IsArrayBuffer(ret));
        p = JS_GetArrayBuffer(ctx, &size, ret);
        assert(size == 0);
        assert(transfer_free_count == 1);
        JS_FreeValue(ctx, ret);

        det = eval(ctx, "ab.detached");
        assert(JS_IsBool(det));
        assert(JS_VALUE_GET_BOOL(det));
        JS_FreeValue(ctx, det);

        ret = eval(ctx, "globalThis.ab = undefined");
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, obj);
        assert(transfer_free_count == 1);
    }

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void resize_external_array_buffer(void)
{
    JSValue obj, global, ret, exc, det;
    const char *s;
    uint8_t *buf, *p;
    size_t size;

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);

    transfer_free_count = 0;
    transfer_realloc_count = 0;
    obj = make_external_ab(ctx, &buf, 8, /*max_len*/16, transfer_realloc_func);

    ret = eval(ctx, "ab.resizable && ab.maxByteLength === 16");
    assert(JS_IsBool(ret));
    assert(JS_VALUE_GET_BOOL(ret));
    JS_FreeValue(ctx, ret);

    // growing reallocs through the callback and zeroes the new tail
    ret = eval(ctx, "ab.resize(16), ab.byteLength");
    assert(!JS_IsException(ret));
    assert(JS_VALUE_GET_INT(ret) == 16);
    JS_FreeValue(ctx, ret);
    assert(transfer_realloc_count == 1);
    p = JS_GetArrayBuffer(ctx, &size, obj);
    assert(size == 16);
    assert(p[0] == 0xAB && p[7] == 0xAB);
    assert(p[8] == 0 && p[15] == 0);

    // and so does shrinking
    ret = eval(ctx, "ab.resize(4), ab.byteLength");
    assert(!JS_IsException(ret));
    assert(JS_VALUE_GET_INT(ret) == 4);
    JS_FreeValue(ctx, ret);
    assert(transfer_realloc_count == 2);
    p = JS_GetArrayBuffer(ctx, &size, obj);
    assert(size == 4);
    assert(p[0] == 0xAB && p[3] == 0xAB);

    // resizing past maxByteLength still throws, without calling the callback
    ret = eval(ctx, "ab.resize(17)");
    assert(JS_IsException(ret));
    JS_FreeValue(ctx, ret);
    exc = JS_GetException(ctx);
    s = JS_ToCString(ctx, exc);
    assert(s);
    assert(strstr(s, "invalid array buffer length"));
    JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, exc);
    assert(transfer_realloc_count == 2);

    // a resizable external AB carries its maxByteLength across a transfer
    ret = eval(ctx, "ab.transfer(8)");
    assert(!JS_IsException(ret));
    assert(transfer_realloc_count == 3);
    det = eval(ctx, "ab.detached");
    assert(JS_IsBool(det));
    assert(JS_VALUE_GET_BOOL(det));
    JS_FreeValue(ctx, det);
    global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "ab2", ret);
    JS_FreeValue(ctx, global);
    ret = eval(ctx, "ab2.resizable && ab2.maxByteLength === 16");
    assert(JS_IsBool(ret));
    assert(JS_VALUE_GET_BOOL(ret));
    JS_FreeValue(ctx, ret);
    ret = eval(ctx, "ab2.resize(16), ab2.byteLength");
    assert(!JS_IsException(ret));
    assert(JS_VALUE_GET_INT(ret) == 16);
    JS_FreeValue(ctx, ret);
    assert(transfer_realloc_count == 4);

    assert(transfer_free_count == 0);
    ret = eval(ctx, "globalThis.ab = globalThis.ab2 = undefined");
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, obj);
    assert(transfer_free_count == 1);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

/* hands the buffer over to the runtime allocator, the way quickjs manages the
   memory of ArrayBuffers it allocates itself */
static void *runtime_realloc_func(JSRuntime *rt, void *opaque, void *ptr,
                                  size_t size)
{
    return js_realloc_rt(rt, ptr, size);
}

static void transfer_default_managed_array_buffer(void)
{
    JSValue obj, global, ret, det;
    uint8_t *buf, *p;
    size_t size;

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);

    buf = js_malloc(ctx, 8);
    memset(buf, 0xAB, 8);
    obj = JS_NewArrayBuffer(ctx, buf, 8, /*max_len*/0,
                            runtime_realloc_func, NULL, false);
    assert(JS_IsArrayBuffer(obj));
    global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "ab", JS_DupValue(ctx, obj));
    JS_FreeValue(ctx, global);

    ret = eval(ctx, "ab.transfer(16)");
    assert(!JS_IsException(ret));
    assert(JS_IsArrayBuffer(ret));
    p = JS_GetArrayBuffer(ctx, &size, ret);
    assert(size == 16);
    assert(p[0] == 0xAB && p[7] == 0xAB);
    assert(p[8] == 0 && p[15] == 0);
    det = eval(ctx, "ab.detached");
    assert(JS_IsBool(det));
    assert(JS_VALUE_GET_BOOL(det));
    JS_FreeValue(ctx, det);
    JS_FreeValue(ctx, ret);
    ret = eval(ctx, "globalThis.ab = undefined");
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, obj);

    buf = js_malloc(ctx, 8);
    memset(buf, 0xCD, 8);
    obj = JS_NewArrayBuffer(ctx, buf, 8, /*max_len*/0,
                            runtime_realloc_func, NULL, false);
    global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "ab", JS_DupValue(ctx, obj));
    JS_FreeValue(ctx, global);

    ret = eval(ctx, "ab.transferToFixedLength(4)");
    assert(!JS_IsException(ret));
    p = JS_GetArrayBuffer(ctx, &size, ret);
    assert(size == 4);
    assert(p[0] == 0xCD && p[3] == 0xCD);
    JS_FreeValue(ctx, ret);
    ret = eval(ctx, "globalThis.ab = undefined");
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, obj);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void get_class_name(void)
{
    static const struct {
        const char *code;
        const char *name;
    } builtins[] = {
        { "({})",               "Object" },
        { "[]",                 "Array" },
        { "new Error()",        "Error" },
        { "new Date()",         "Date" },
        { "/re/",               "RegExp" },
        { "new Map()",          "Map" },
        { "new ArrayBuffer(0)", "ArrayBuffer" },
        { "new Uint8Array(0)",  "Uint8Array" },
        { "(function(){})",     "Function" },
    };
    JSClassDef def = (JSClassDef){ .class_name = "MyClass" };
    JSClassID class_id, unregistered_class_id;
    JSAtom atom, expected;
    JSRuntime *rt;
    JSContext *ctx;
    const char *s;
    JSValue obj;
    size_t i;

    rt = new_runtime();
    class_id = 0;
    JS_NewClassID(rt, &class_id);
    assert(0 == JS_NewClass(rt, class_id, &def));

    /* handed out by JS_NewClassID() but never passed to JS_NewClass() */
    unregistered_class_id = 0;
    JS_NewClassID(rt, &unregistered_class_id);

    ctx = JS_NewContext(rt);

    /* the name of a class registered from C, not its class id */
    expected = JS_NewAtom(ctx, "MyClass");
    atom = JS_GetClassName(rt, class_id);
    assert(atom == expected);
    s = JS_AtomToCString(ctx, atom);
    assert(s);
    assert(!strcmp(s, "MyClass"));
    JS_FreeCString(ctx, s);
    JS_FreeAtom(ctx, atom);
    JS_FreeAtom(ctx, expected);

    /* the names of the built-in classes */
    for (i = 0; i < countof(builtins); i++) {
        obj = eval(ctx, builtins[i].code);
        assert(!JS_IsException(obj));
        atom = JS_GetClassName(rt, JS_GetClassID(obj));
        assert(atom != JS_ATOM_NULL);
        s = JS_AtomToCString(ctx, atom);
        assert(s);
        assert(!strcmp(s, builtins[i].name));
        JS_FreeCString(ctx, s);
        JS_FreeAtom(ctx, atom);
        JS_FreeValue(ctx, obj);
    }

    /* class ids without a registered class have no name */
    assert(JS_ATOM_NULL == JS_GetClassName(rt, JS_INVALID_CLASS_ID));
    assert(JS_ATOM_NULL == JS_GetClassName(rt, unregistered_class_id));
    assert(JS_ATOM_NULL == JS_GetClassName(rt, class_id + 4096));

    /* the caller owns the returned atom; the class keeps its own reference,
       so releasing it repeatedly doesn't free the name out from under it */
    for (i = 0; i < 64; i++)
        JS_FreeAtom(ctx, JS_GetClassName(rt, class_id));
    atom = JS_GetClassName(rt, class_id);
    s = JS_AtomToCString(ctx, atom);
    assert(s);
    assert(!strcmp(s, "MyClass"));
    JS_FreeCString(ctx, s);
    JS_FreeAtom(ctx, atom);

    /* every registered class has a name, and it is a name rather than a
       number: returning the class id instead produced a valid-looking atom,
       either an unrelated predefined one for a small id or the id spelled out
       in decimal for a large one, so check the whole table rather than the
       handful of classes spelled out above */
    {
        JSClassID id;
        int registered = 0;

        for (id = 1; id < class_id + 16; id++) {
            char decimal[32];
            if (!JS_IsRegisteredClass(rt, id))
                continue;
            registered++;
            atom = JS_GetClassName(rt, id);
            assert(atom != JS_ATOM_NULL);
            s = JS_AtomToCString(ctx, atom);
            assert(s);
            // a few internal classes are deliberately unnamed, but none is
            // named after a number
            snprintf(decimal, sizeof(decimal), "%u", (unsigned)id);
            assert(strcmp(s, decimal));
            assert(s[0] < '0' || s[0] > '9');
            JS_FreeCString(ctx, s);
            JS_FreeAtom(ctx, atom);
        }
        /* the built-ins alone are far more than this */
        assert(registered > 20);
    }

    /* two classes sharing a name share the interned atom, and the name does
       not depend on which context asks */
    {
        JSClassDef def2 = (JSClassDef){ .class_name = "MyClass" };
        JSClassID class_id2 = 0;
        JSContext *ctx2;
        JSAtom a1, a2;

        JS_NewClassID(rt, &class_id2);
        assert(0 == JS_NewClass(rt, class_id2, &def2));
        assert(class_id2 != class_id);

        a1 = JS_GetClassName(rt, class_id);
        a2 = JS_GetClassName(rt, class_id2);
        assert(a1 == a2);
        JS_FreeAtom(ctx, a1);
        JS_FreeAtom(ctx, a2);

        ctx2 = JS_NewContext(rt);
        a1 = JS_GetClassName(rt, class_id);
        a2 = JS_GetClassName(rt, class_id);
        assert(a1 == a2);
        JS_FreeAtom(ctx2, a1);
        JS_FreeAtom(ctx2, a2);
        JS_FreeContext(ctx2);
    }

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

void object_from(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue t0 = JS_NewObject(ctx);
    assert(!JS_IsException(t0));
    assert(JS_IsObject(t0));
    JSAtom prop = JS_NewAtomLen(ctx, "prop", 4);
    assert(prop != JS_ATOM_NULL);
    JSValue val = JS_NULL;
    JSValue t1 = JS_NewObjectFrom(ctx, 1, &prop, &val);
    assert(!JS_IsException(t1));
    assert(JS_IsObject(t1));
    uint32_t old_shape_hash_count = js_std_cmd(/*GetShapeHashCount*/4, rt);
    JSValue t2 = JS_NewObjectFrom(ctx, 1, &prop, &val);
    assert(!JS_IsException(t2));
    assert(JS_IsObject(t2));
    uint32_t new_shape_hash_count = js_std_cmd(/*GetShapeHashCount*/4, rt);
    assert(old_shape_hash_count == new_shape_hash_count);
    JS_FreeAtom(ctx, prop);
    JS_FreeValue(ctx, t2);
    JS_FreeValue(ctx, t1);
    JS_FreeValue(ctx, t0);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

void add_intrinsic_bigint(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContextRaw(rt);
    JS_AddIntrinsicBaseObjects(ctx);
    JS_AddIntrinsicBigInt(ctx);
    assert(!JS_HasException(ctx));
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

void new_typed_array(void)
{
    JSValueConst argv[4];
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    uint8_t *buf = js_malloc(ctx, 8);
    JSValue ab = JS_NewArrayBuffer(ctx, buf, 8, /*max_len*/0, NULL, NULL, false);
    assert(JS_IsArrayBuffer(ab));
    for (int argc = 0; argc < (int)countof(argv); argc++) {
        JSValue ret = JS_NewTypedArray(ctx, argc, argv, JS_TYPED_ARRAY_UINT8);
        assert(!JS_IsException(ret));
        assert(JS_IsObject(ret));
        JS_FreeValue(ctx, ret);
        argv[argc] = JS_UNDEFINED;
        argv[0] = ab;
    }
    JS_FreeValue(ctx, ab);
    js_free(ctx, buf);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

void private_symbols(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue pub = JS_NewSymbol(ctx, "public", /*is_global*/false);
    JSValue priv = JS_NewPrivateSymbol(ctx, "private");
    assert(JS_IsSymbol(pub));
    assert(JS_IsSymbol(priv));
    JSValue obj = JS_NewObject(ctx);
    assert(JS_IsObject(obj));
    assert(true == JS_SetPropertyValue(ctx, obj, pub, JS_TRUE, JS_PROP_C_W_E));
    assert(true == JS_SetPropertyValue(ctx, obj, priv, JS_FALSE, JS_PROP_C_W_E));
    JSValue global_object = JS_GetGlobalObject(ctx);
    assert(true == JS_SetPropertyStr(ctx, global_object, "o", JS_DupValue(ctx, obj)));
    JS_FreeValue(ctx, global_object);
    JSValue result = eval(ctx, "Object.getOwnPropertySymbols(o)");
    assert(JS_IsArray(result));
    int64_t length = -1;
    assert(0 == JS_GetLength(ctx, result, &length));
    assert(length == 1);
    JSValue item = JS_GetPropertyUint32(ctx, result, 0);
    assert(JS_IsSymbol(item));
    assert(JS_IsSameValue(ctx, item, pub));
    JS_FreeValue(ctx, item);
    JS_FreeValue(ctx, result);
    result = JS_GetPropertyValue(ctx, obj, JS_DupValue(ctx, pub));
    assert(JS_IsBool(result));
    assert(JS_IsSameValue(ctx, result, JS_TRUE));
    result = JS_GetPropertyValue(ctx, obj, JS_DupValue(ctx, priv));
    assert(JS_IsBool(result));
    assert(JS_IsSameValue(ctx, result, JS_FALSE));
    JS_FreeValue(ctx, obj);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static unsigned execution_active_calls;

static JSValue execution_query(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv)
{
    bool active = JS_IsExecutingJS(JS_GetRuntime(ctx));
    if (active)
        execution_active_calls++;
    return JS_NewBool(ctx, active);
}

static JSValue execution_query_data(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv,
                                    int magic, JSValueConst *data)
{
    return execution_query(ctx, this_val, argc, argv);
}

static JSValue execution_query_closure(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv,
                                       int magic, void *opaque)
{
    /* Execution in this runtime must not affect another runtime. */
    assert(!JS_IsExecutingJS(opaque));
    return execution_query(ctx, this_val, argc, argv);
}

static JSValue execution_job(JSContext *ctx, int argc, JSValueConst *argv)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    int mode;

    assert(JS_IsExecutingJS(rt)); /* no bytecode frames in these jobs */
    assert(argc == 1);
    assert(0 == JS_ToInt32(ctx, &mode, argv[0]));
    if (mode == 2)
        return JS_Throw(ctx, JS_NewInt32(ctx, 42));
    if (mode == 1) {
        JSContext *job_ctx;
        JSValue arg = JS_NewInt32(ctx, 2);
        assert(0 == JS_EnqueueJob(ctx, execution_job, 1, &arg));
        assert(-1 == JS_ExecutePendingJob(rt, &job_ctx));
        assert(job_ctx == ctx);
        assert(JS_IsExecutingJS(rt)); /* nested error did not end outer job */
        assert(JS_HasException(ctx));
        JSValue exception = JS_GetException(ctx);
        assert(JS_IsSameValue(ctx, exception, JS_NewInt32(ctx, 42)));
        JS_FreeValue(ctx, exception);
    }
    return JS_UNDEFINED;
}

static int execution_interrupt(JSRuntime *rt, void *opaque)
{
    /* The first poll can happen before the interpreter pushes its frame. */
    return JS_IsExecutingJS(rt);
}

static void assert_execution_eval(JSContext *ctx, const char *code)
{
    JSValue result = eval(ctx, code);
    assert(!JS_IsException(result));
    assert(1 == JS_ToBool(ctx, result));
    JS_FreeValue(ctx, result);
    assert(!JS_IsExecutingJS(JS_GetRuntime(ctx)));
}

static void is_executing_js(void)
{
    JSRuntime *rt = new_runtime();
    JSRuntime *idle_rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSContext *other = JS_NewContext(rt);
    JSValue funcs[] = {
        JS_NewCFunction(ctx, execution_query, "executing", 0),
        JS_NewCFunctionData(ctx, execution_query_data, 0, 0, 0, NULL),
        JS_NewCClosure(ctx, execution_query_closure, "executingClosure", NULL,
                       0, 0, idle_rt),
    };
    const char *names[] = {"executing", "executingData", "executingClosure"};
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue other_global = JS_GetGlobalObject(other);
    JSValue result;
    JSContext *job_ctx;

    assert(!JS_IsExecutingJS(rt));
    for (int i = 0; i < (int)countof(funcs); i++) {
        /* A C frame alone must not make the query report JavaScript. */
        result = JS_Call(ctx, funcs[i], JS_UNDEFINED, 0, NULL);
        assert(JS_IsBool(result) && 0 == JS_ToBool(ctx, result));
        JS_FreeValue(ctx, result);
        assert(1 == JS_SetPropertyStr(ctx, global, names[i], funcs[i]));
    }
    assert(1 == JS_SetPropertyStr(other, other_global, "executing",
                                  JS_GetPropertyStr(ctx, global, "executing")));
    assert_execution_eval(ctx, "executing() && executingData() && executingClosure()");
    assert_execution_eval(other, "executing()"); /* creation realm differs */
    assert_execution_eval(ctx,
        "Object.defineProperty({}, 'x', {get: executing}).x && "
        "executing.call(null) && executing.apply(null, []) && executing.bind(null)()");

    /* Call every bytecode-function class from C, without an eval frame below
       it that could hide a missing generator/async-function classification. */
    const char *sources[] = {
        "(() => executing())",
        "(async () => executing())",
        "(function* () {yield executing()})",
        "(async function* () {yield executing()})",
    };
    for (int i = 0; i < (int)countof(sources); i++) {
        JSValue func = eval(ctx, sources[i]);
        unsigned before = execution_active_calls;
        result = JS_Call(ctx, func, JS_UNDEFINED, 0, NULL);
        assert(!JS_IsException(result));
        JS_FreeValue(ctx, func);
        if (i >= 2) {
            JSValue next = JS_GetPropertyStr(ctx, result, "next");
            JSValue step = JS_Call(ctx, next, result, 0, NULL);
            assert(!JS_IsException(step));
            JS_FreeValue(ctx, step);
            JS_FreeValue(ctx, next);
        }
        JS_FreeValue(ctx, result);
        while (JS_ExecutePendingJob(rt, &job_ctx) > 0)
            assert(!JS_IsExecutingJS(rt));
        assert(execution_active_calls == before + 1);
        assert(!JS_IsExecutingJS(rt));
    }

    /* A C-driven builtin plus native callback still has no bytecode frame. */
    result = eval(ctx, "executing.bind(null)");
    JSValue called = JS_Call(ctx, result, JS_UNDEFINED, 0, NULL);
    assert(JS_IsBool(called) && 0 == JS_ToBool(ctx, called));
    JS_FreeValue(ctx, called);
    JS_FreeValue(ctx, result);

    assert_execution_eval(ctx,
        "globalThis.g = (function*() {yield executing(); return executing()})(); "
        "g.next().value === true");
    assert_execution_eval(ctx, "g.next().value === true");
    assert_execution_eval(ctx,
        "globalThis.asyncActive = false; "
        "(async () => {await 0; asyncActive = executing()})(); true");
    assert(JS_IsJobPending(rt));
    assert(!JS_IsExecutingJS(rt));
    while (JS_ExecutePendingJob(rt, &job_ctx) > 0)
        assert(!JS_IsExecutingJS(rt));
    assert_execution_eval(ctx, "asyncActive === true");

    for (int mode = 0; mode < 3; mode++) {
        JSValue arg = JS_NewInt32(ctx, mode);
        assert(0 == JS_EnqueueJob(ctx, execution_job, 1, &arg));
        assert(!JS_IsExecutingJS(rt)); /* queued is not executing */
        assert((mode == 2 ? -1 : 1) == JS_ExecutePendingJob(rt, &job_ctx));
        assert(!JS_IsExecutingJS(rt));
        if (mode == 2) {
            assert(JS_HasException(ctx));
            result = JS_GetException(ctx);
            assert(JS_IsSameValue(ctx, result, JS_NewInt32(ctx, 42)));
            JS_FreeValue(ctx, result);
        }
    }

    result = JS_NewObject(ctx);
    JS_Throw(ctx, JS_DupValue(ctx, result));
    assert(!JS_IsExecutingJS(rt));
    assert(JS_HasException(ctx));
    called = JS_GetException(ctx);
    assert(JS_IsSameValue(ctx, result, called));
    JS_FreeValue(ctx, called);
    JS_FreeValue(ctx, result);

    JS_SetInterruptHandler(rt, execution_interrupt, NULL);
    result = eval(ctx, "for (;;) {}");
    assert(JS_IsException(result));
    assert(!JS_IsExecutingJS(rt));
    assert(JS_HasException(ctx));
    result = JS_GetException(ctx);
    assert(JS_IsUncatchableError(result));
    JS_FreeValue(ctx, result);
    JS_SetInterruptHandler(rt, NULL, NULL);

    JS_FreeValue(other, other_global);
    JS_FreeValue(ctx, global);
    JS_FreeContext(other);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    JS_FreeRuntime(idle_rt);
}

static JSClassID lazy_marker_class;
static unsigned lazy_marker_finalized;
static unsigned lazy_sab_live;

typedef struct LazySharedBuffer {
    size_t refs;
    uint8_t data[];
} LazySharedBuffer;

static void *lazy_sab_alloc(void *opaque, size_t size)
{
    LazySharedBuffer *buffer = malloc(sizeof(*buffer) + size);
    if (!buffer)
        return NULL;
    buffer->refs = 1;
    lazy_sab_live++;
    return buffer->data;
}

static void lazy_sab_dup(void *opaque, void *data)
{
    LazySharedBuffer *buffer = (LazySharedBuffer *)((uint8_t *)data - sizeof(*buffer));
    assert(buffer->refs > 0);
    buffer->refs++;
}

static void lazy_sab_free(void *opaque, void *data)
{
    LazySharedBuffer *buffer = (LazySharedBuffer *)((uint8_t *)data - sizeof(*buffer));
    assert(buffer->refs > 0);
    if (--buffer->refs == 0) {
        lazy_sab_live--;
        free(buffer);
    }
}

static void lazy_marker_finalizer(JSRuntime *rt, JSValueConst object)
{
    lazy_marker_finalized++;
}

static JSValue lazy_marker(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv)
{
    return JS_NewObjectClass(ctx, lazy_marker_class);
}

static JSValue lazy_define(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv)
{
    JSAtom atom;
    int flags = JS_PROP_C_W_E, ret;

    assert(argc >= 3);
    if (argc > 3 && JS_ToInt32(ctx, &flags, argv[3]))
        return JS_EXCEPTION;
    atom = JS_ValueToAtom(ctx, argv[1]);
    if (atom == JS_ATOM_NULL)
        return JS_EXCEPTION;
    ret = JS_DefineLazyDataProperty(ctx, argv[0], atom, argv[2], flags);
    JS_FreeAtom(ctx, atom);
    return ret < 0 ? JS_EXCEPTION : JS_NewBool(ctx, ret);
}

static JSValue lazy_query(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv)
{
    JSAtom atom;
    bool lazy;

    assert(argc >= 2);
    atom = JS_ValueToAtom(ctx, argv[1]);
    if (atom == JS_ATOM_NULL)
        return JS_EXCEPTION;
    lazy = JS_IsLazyDataProperty(ctx, argv[0], atom);
    JS_FreeAtom(ctx, atom);
    return JS_NewBool(ctx, lazy);
}

static JSValue lazy_gc(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv)
{
    JS_RunGC(JS_GetRuntime(ctx));
    return JS_UNDEFINED;
}

static JSValue lazy_clone(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv)
{
    size_t size;
    JSSABTab table;
    uint8_t *data = JS_WriteObject2(ctx, &size, argv[0],
                                  JS_WRITE_OBJ_REFERENCE | JS_WRITE_OBJ_SAB | JS_WRITE_OBJ_SAB_RETAIN,
                                  &table);
    if (!data)
        return JS_EXCEPTION;
    JSValue result = JS_ReadObject(ctx, data, size, JS_READ_OBJ_REFERENCE | JS_READ_OBJ_SAB);
    JS_FreeSABTab(ctx, &table);
    js_free(ctx, data);
    return result;
}

static JSContext *lazy_native_expected_context;
static unsigned lazy_exotic_calls;

static int lazy_exotic_get(JSContext *ctx, JSPropertyDescriptor *desc,
                            JSValueConst object, JSAtom prop)
{
    lazy_exotic_calls++;
    JS_ThrowTypeError(ctx, "unexpected exotic property lookup");
    return -1;
}

static JSValue lazy_native_initializer(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv,
                                       int magic, JSValueConst *data)
{
    assert(ctx == lazy_native_expected_context);
    JSAtom atom = JS_ValueToAtom(ctx, argv[0]);
    JSValue result;
    if (magic) {
        result = JS_GetProperty(ctx, this_val, atom);
    } else {
        int ret = JS_SetProperty(ctx, this_val, atom, JS_NewInt32(ctx, 8));
        result = ret < 0 ? JS_EXCEPTION : JS_NewInt32(ctx, 42);
    }
    JS_FreeAtom(ctx, atom);
    return result;
}

static void assert_lazy_eval(JSContext *ctx, const char *code)
{
    JSValue result = eval(ctx, code);
    if (JS_IsException(result)) {
        fprintf(stderr, "lazy-property regression failed: %s\n", code);
        js_std_dump_error(ctx);
        assert(0);
    }
    assert(1 == JS_ToBool(ctx, result));
    JS_FreeValue(ctx, result);
}

static void lazy_data_properties(void)
{
    static const char *const tests[] = {
        "(() => {"
        " let n=0; const o={before:1}, s=Symbol('lazy');"
        " if(!lazy(o,'x',function(k){++n;return this===o&&k==='x'&&isLazy(o,k)?42:0}))return false;"
        " lazy(o,s,function(k){++n;return k===s?43:0}); o.after=2;"
        " const child=Object.create(o); let keys=[]; for(const k in o)keys.push(k);"
        " if(!('x' in child)||!o.hasOwnProperty('x')||child.hasOwnProperty('x')||isLazy(child,'x'))return false;"
        " if(Object.keys(o).join()!=='before,x,after'||keys.join()!=='before,x,after'||n)return false;"
        " if(Reflect.ownKeys(o).at(-1)!==s||!isLazy(o,s))return false;"
        " if(child.x!==42||o.x!==42||isLazy(o,'x')||o[s]!==43||n!==2)return false;"
        " const d=Object.getOwnPropertyDescriptor(o,'x');"
        " return d.value===42&&d.writable&&d.enumerable&&d.configurable&&!('get' in d); })()",

        "(() => { for(let flags=0;flags<8;++flags){"
        " const o={};let n=0;lazy(o,'x',()=>++n,flags);"
        " if(!isLazy(o,'x'))return false;"
        " const written=Reflect.set(o,'x',9);"
        " if(written!==!!(flags&2)||n)return false;"
        " if(written){if(isLazy(o,'x')||o.x!==9)return false;}"
        " else {let threw=false;try{(function(){'use strict';o.x=9})()}catch(e){threw=e instanceof TypeError}"
        " if(!threw||!isLazy(o,'x')||n)return false;}"
        " const d=Object.getOwnPropertyDescriptor(o,'x');"
        " if(d.value!==(written?9:1)||d.configurable!==!!(flags&1)||d.writable!==!!(flags&2)||d.enumerable!==!!(flags&4))return false;"
        " const p={};lazy(p,'y',()=>{throw 1},flags);"
        " if(Reflect.deleteProperty(p,'y')!==!!(flags&1)||isLazy(p,'y')===!!(flags&1))return false;"
        " }return true;})()",

        "(() => {let n=0;const p={};lazy(p,'x',()=>++n);const c=Object.create(p);"
        " if(!Reflect.set(p,'x',8,c)||n||c.x!==8||!isLazy(p,'x'))return false;"
        " const r={};lazy(r,'x',()=>++n);if(!Reflect.set(p,'x',9,r)||n||r.x!==9||isLazy(r,'x'))return false;"
        " const q={};lazy(q,'x',()=>++n,0);const d=Object.create(q);"
        " return !Reflect.set(q,'x',7,d)&&!d.hasOwnProperty('x')&&isLazy(q,'x')&&n===0;})()",

        "(() => {let n=0;const o={a:1,get x(){throw 99},b:2};"
        " if(!lazy(o,'x',()=>++n)||Object.keys(o).join()!=='a,x,b'||n)return false;"
        " if(!lazy(o,'x',()=>42)||n||o.x!==42)return false;"
        " const d=Object.getOwnPropertyDescriptor(o,'x');"
        " Object.preventExtensions(o);if(!lazy(o,'x',()=>43)||o.x!==43)return false;"
        " Object.defineProperty(o,'x',{configurable:false});"
        " return !lazy(o,'x',()=>44)&&!lazy(o,'new',()=>0)&&o.x===43;})()",

        "(() => {let n=0;const o={},error={};lazy(o,'x',()=>{if(++n===1)throw error;return 42});"
        " try{Object.getOwnPropertyDescriptor(o,'x');return false}catch(e){if(e!==error)return false}"
        " return isLazy(o,'x')&&o.x===42&&n===2&&!isLazy(o,'x');})()",

        "(() => {const o={};lazy(o,'x',()=>o.x);"
        " try{o.x;return false}catch(e){if(!(e instanceof RangeError))return false}"
        " if(!isLazy(o,'x'))return false;"
        " lazy(o,'x',()=>{try{Object.defineProperty(o,'x',{value:8})}catch(e){return e instanceof RangeError?42:0}});"
        " return o.x===42&&!isLazy(o,'x');})()",

        "(() => {for(const action of [o=>o.x=8,o=>delete o.x,o=>lazy(o,'x',()=>9)]){"
        " const o={};lazy(o,'x',()=>{action(o);gcLazy();return 42});"
        " try{o.x;return false}catch(e){if(!(e instanceof TypeError))return false}"
        " if(o.hasOwnProperty('x')&&o.x!==8&&o.x!==9)return false;"
        " }const o={};let recursive=false;"
        " lazy(o,'x',()=>{lazy(o,'x',()=>9);try{o.x}catch(e){recursive=e instanceof RangeError}return 42});"
        " try{o.x;return false}catch(e){if(!(e instanceof TypeError))return false}"
        " return recursive&&isLazy(o,'x')&&o.x===9;})()",

        "(() => {const o={},error={};lazy(o,'x',function(){this.x=8;throw error});"
        " try{o.x;return false}catch(e){if(e!==error)return false}"
        " return o.x===8&&!isLazy(o,'x');})()",

        "(() => {const o={before:1};lazy(o,'x',function(){"
        " for(let i=0;i<1000;i++)this['grow'+i]=i;"
        " for(let i=0;i<900;i++)delete this['grow'+i];gcLazy();return 42});"
        " return o.x===42&&o.grow999===999&&Object.keys(o).slice(0,2).join()==='before,x';})()",

        "(() => {let child=Object.create({tag:42});"
        " lazy(Object.getPrototypeOf(child),'x',function(){Object.setPrototypeOf(child,null);gcLazy();return this.tag});"
        " return child.x===42&&Object.getPrototypeOf(child)===null;})()",

        "(() => {const o={};let n=0;lazy(o,'a',()=>++n);lazy(o,'b',()=>++n);"
        " const d=Object.getOwnPropertyDescriptors(o);"
        " return d.a.value===1&&d.b.value===2&&n===2&&!isLazy(o,'a')&&!isLazy(o,'b');})()",

        "(() => {const o={};let n=0;lazy(o,'x',()=>++n);Object.freeze(o);"
        " return isLazy(o,'x')&&n===0&&Object.isFrozen(o)&&o.x===1&&!isLazy(o,'x');})()",

        "(() => {let traps=0;const o={},p=new Proxy(o,{getOwnPropertyDescriptor(){++traps;throw 0},defineProperty(){++traps;throw 0}});"
        " lazy(o,'x',()=>42);const revoked=Proxy.revocable(o,{});revoked.revoke();"
        " return !isLazy(p,'x')&&!isLazy(revoked.proxy,'x')&&!lazy(p,'x',()=>0)&&traps===0&&isLazy(o,'x');})()",

        "(() => {const a=new Uint8Array(4);"
        " for(const key of ['0','4294967294','-0','NaN','Infinity','-Infinity','-1','1.5','4294967295','1e+21'])"
        " if(lazy(a,key,()=>1))throw Error('accepted numeric key '+key);"
        " for(const key of ['01','1e21','+1','0.10','-','Infinity!'])"
        " if(!lazy(a,key,()=>42)||a[key]!==42)throw Error('rejected ordinary key '+key);"
        " const o={};if(lazy(o,'0',()=>1)||lazy(o,'4294967294',()=>1))return false;"
        " return lazy(o,'4294967295',()=>42)&&o['4294967295']===42;})()",

        "(() => {const o={};lazy(o,'x',()=>({answer:42}));gcLazy();"
        " const value=o.x;gcLazy();return value===o.x&&value.answer===42;})()",

        "(() => {const o={};let n=0;lazy(o,'x',()=>++n);lazy(o,'hidden',()=>++n,0);"
        " const c=cloneLazy(o);return c.x===1&&n===1&&!isLazy(o,'x')&&isLazy(o,'hidden')&&!('hidden' in c);})()",

        "(() => {const o={};lazy(o,'first',function(){delete this.later;"
        " for(let i=0;i<1000;i++)this['new'+i]=i;gcLazy();return 42});o.later=9;o.last=8;"
        " const c=cloneLazy(o);return c.first===42&&c.last===8&&!('later' in c)&&!('new0' in c);})()",

        "(() => {const o={first:{}};const nested={};"
        " lazy(nested,'x',()=>{delete o.first;o.later=42;gcLazy();return 8});o.nested=nested;o.later=9;"
        " const c=cloneLazy(o);return typeof c.first==='object'&&c.nested.x===8&&c.later===42;})()",

        "(() => {const o={};for(let i=0;i<150;i++)lazy(o,'key'+i,()=>i);"
        " const c=cloneLazy(o);return Object.keys(c).length===150&&c.key149===149;})()",

        "(() => {const error={},o={};lazy(o,'x',()=>{throw error});"
        " try{cloneLazy(o);return false}catch(e){return e===error&&isLazy(o,'x')}})()",

        "(() => {let n=0;const o={get x(){++n;return 42}};"
        " try{cloneLazy(o);return false}catch(e){return e instanceof TypeError&&n===0}})()",

        "(() => {const o={},m=new Map([[1,o],[2,9]]);"
        " lazy(o,'x',()=>{m.clear();gcLazy();return 42});const c=cloneLazy(m);"
        " return m.size===0&&c.size===2&&c.get(1).x===42&&c.get(2)===9;})()",

        "(() => {const o={},s=new Set([o,9]);"
        " lazy(o,'x',()=>{s.clear();gcLazy();return 42});const c=cloneLazy(s);"
        " return s.size===0&&c.size===2&&[...c][0].x===42&&c.has(9);})()",

        "(() => {const o={sab:new SharedArrayBuffer(8)};new Uint8Array(o.sab)[0]=42;"
        " lazy(o,'x',function(){delete this.sab;gcLazy();return 9});const c=cloneLazy(o);"
        " return !('sab' in o)&&c.x===9&&new Uint8Array(c.sab)[0]===42;})()",

        "(() => {const error={},o={sab:new SharedArrayBuffer(8)};"
        " lazy(o,'x',function(){delete this.sab;gcLazy();throw error});"
        " try{cloneLazy(o);return false}catch(e){return e===error&&isLazy(o,'x')}})()",
    };
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue global = JS_GetGlobalObject(ctx);
    JSClassDef marker_def = { .class_name = "LazyMarker", .finalizer = lazy_marker_finalizer };
    JSSharedArrayBufferFunctions sab_funcs = {
        .sab_alloc = lazy_sab_alloc, .sab_free = lazy_sab_free, .sab_dup = lazy_sab_dup,
    };

    lazy_sab_live = 0;
    JS_SetSharedArrayBufferFunctions(rt, &sab_funcs);
    lazy_marker_class = 0;
    lazy_marker_finalized = 0;
    JS_NewClassID(rt, &lazy_marker_class);
    assert(0 == JS_NewClass(rt, lazy_marker_class, &marker_def));
    JS_SetPropertyStr(ctx, global, "lazy", JS_NewCFunction(ctx, lazy_define, "lazy", 4));
    JS_SetPropertyStr(ctx, global, "isLazy", JS_NewCFunction(ctx, lazy_query, "isLazy", 2));
    JS_SetPropertyStr(ctx, global, "gcLazy", JS_NewCFunction(ctx, lazy_gc, "gcLazy", 0));
    JS_SetPropertyStr(ctx, global, "lazyMarker", JS_NewCFunction(ctx, lazy_marker, "lazyMarker", 0));
    JS_SetPropertyStr(ctx, global, "cloneLazy", JS_NewCFunction(ctx, lazy_clone, "cloneLazy", 1));
    for (size_t i = 0; i < countof(tests); i++)
        assert_lazy_eval(ctx, tests[i]);
    JS_RunGC(rt);
    assert(lazy_sab_live == 0);

    /* The original non-retaining API keeps its borrowed-table ownership. */
    JSValue borrowed_sab = eval(ctx, "new SharedArrayBuffer(8)");
    size_t borrowed_size;
    JSSABTab borrowed_tab;
    uint8_t *borrowed_data = JS_WriteObject2(ctx, &borrowed_size, borrowed_sab,
                                            JS_WRITE_OBJ_SAB | JS_WRITE_OBJ_REFERENCE,
                                            &borrowed_tab);
    assert(borrowed_data && borrowed_tab.len == 1 && lazy_sab_live == 1);
    LazySharedBuffer *backing = (LazySharedBuffer *)(borrowed_tab.tab[0] - sizeof(*backing));
    assert(backing->refs == 1);
    js_free(ctx, borrowed_tab.tab);
    js_free(ctx, borrowed_data);
    JS_FreeValue(ctx, borrowed_sab);
    assert(lazy_sab_live == 0);

    /* Trace the initializer's closed-over object/holder cycle, and collect it
       when no ordinary root remains. The extra marker detects a leak without
       depending on implementation-specific GC object counts. */
    assert_lazy_eval(ctx,
        "(() => {const o={},m=lazyMarker();lazy(o,'x',()=>[o,m]);return true})()");
    JS_RunGC(rt);
    assert(lazy_marker_finalized == 1);

    /* Defining in a temporary realm must not retain that realm if the holder
       and initializer both belong to the principal realm. */
    JSContext *other = JS_NewContext(rt);
    JSValue other_global = JS_GetGlobalObject(other);
    JSValue object = JS_NewObject(ctx);
    JSValue initializer = eval(ctx, "(function(){return 42})");
    JSAtom atom = JS_NewAtom(ctx, "cross");
    JS_SetPropertyStr(other, other_global, "marker", JS_NewObjectClass(other, lazy_marker_class));
    assert(1 == JS_DefineLazyDataProperty(other, object, atom, initializer, JS_PROP_C_W_E));
    JS_FreeValue(ctx, initializer);
    JS_FreeValue(other, other_global);
    JS_FreeContext(other);
    JS_RunGC(rt);
    assert(lazy_marker_finalized == 2);
    assert(JS_IsLazyDataProperty(ctx, object, atom));
    JSValue value = JS_GetProperty(ctx, object, atom);
    int32_t answer;
    assert(0 == JS_ToInt32(ctx, &answer, value) && answer == 42);
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, object);

    /* Conversely, a live initializer retains and runs in its own realm even
       after the embedder releases that realm's context handle. */
    other = JS_NewContext(rt);
    other_global = JS_GetGlobalObject(other);
    JS_SetPropertyStr(other, other_global, "realmMarker", JS_NewObjectClass(other, lazy_marker_class));
    initializer = eval(other, "(function(name){return [globalThis.realmMarker,name,Array.prototype]})");
    JSValue other_proto = eval(other, "Array.prototype");
    object = JS_NewObject(ctx);
    assert(1 == JS_DefineLazyDataProperty(ctx, object, atom, initializer, JS_PROP_C_W_E));
    JS_FreeValue(other, initializer);
    JS_FreeValue(other, other_global);
    JS_FreeContext(other);
    JS_RunGC(rt);
    assert(lazy_marker_finalized == 2);
    value = JS_GetProperty(ctx, object, atom);
    assert(!JS_IsException(value));
    JSValue result_proto = JS_GetPropertyUint32(ctx, value, 2);
    assert(JS_IsSameValue(ctx, other_proto, result_proto));
    JSValue result_name = JS_GetPropertyUint32(ctx, value, 1);
    const char *name = JS_ToCString(ctx, result_name);
    assert(name && !strcmp(name, "cross"));
    JS_FreeCString(ctx, name);
    JS_FreeValue(ctx, result_name);
    JS_FreeValue(ctx, result_proto);
    JS_FreeValue(ctx, other_proto);
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, object);
    JS_RunGC(rt);
    assert(lazy_marker_finalized == 3);

    /* The read happens in ctx, while errors created by initialization belong
       to the initializer's realm, just like the callback's own errors. */
    other = JS_NewContext(rt);
    object = JS_NewObject(ctx);
    const char *error_sources[] = {
        "(function(k){this[k]=8;return 42})",
        "(function(k){return this[k]})",
    };
    const char *error_prototypes[] = { "TypeError.prototype", "RangeError.prototype" };
    for (size_t i = 0; i < countof(error_sources); i++) {
        initializer = eval(other, error_sources[i]);
        assert(1 == JS_DefineLazyDataProperty(ctx, object, atom, initializer, JS_PROP_C_W_E));
        JS_FreeValue(other, initializer);
        value = JS_GetProperty(ctx, object, atom);
        assert(JS_IsException(value));
        JSValue error = JS_GetException(ctx);
        JSValue error_proto = JS_GetPrototype(ctx, error);
        JSValue expected_proto = eval(other, error_prototypes[i]);
        assert(JS_IsSameValue(ctx, expected_proto, error_proto));
        JS_FreeValue(ctx, error);
        JS_FreeValue(ctx, error_proto);
        JS_FreeValue(other, expected_proto);
    }
    lazy_native_expected_context = other;
    for (size_t i = 0; i < countof(error_sources); i++) {
        initializer = JS_NewCFunctionData(other, lazy_native_initializer, 1, i, 0, NULL);
        assert(1 == JS_DefineLazyDataProperty(ctx, object, atom, initializer, JS_PROP_C_W_E));
        JS_FreeValue(other, initializer);
        value = JS_GetProperty(ctx, object, atom);
        assert(JS_IsException(value));
        JSValue error = JS_GetException(ctx);
        JSValue error_proto = JS_GetPrototype(ctx, error);
        JSValue expected_proto = eval(other, error_prototypes[i]);
        assert(JS_IsSameValue(ctx, expected_proto, error_proto));
        JS_FreeValue(ctx, error);
        JS_FreeValue(ctx, error_proto);
        JS_FreeValue(other, expected_proto);
    }
    JS_FreeValue(ctx, object);
    JS_FreeContext(other);

    /* Query/definition must reject embedder exotics without entering them. */
    JSClassID exotic_class = 0;
    JSClassExoticMethods exotic_methods = { .get_own_property = lazy_exotic_get };
    JSClassDef exotic_def = { .class_name = "LazyExotic", .exotic = &exotic_methods };
    JS_NewClassID(rt, &exotic_class);
    assert(0 == JS_NewClass(rt, exotic_class, &exotic_def));
    object = JS_NewObjectClass(ctx, exotic_class);
    initializer = eval(ctx, "(function(){return 42})");
    lazy_exotic_calls = 0;
    assert(!JS_IsLazyDataProperty(ctx, object, atom));
    assert(0 == JS_DefineLazyDataProperty(ctx, object, atom, initializer, JS_PROP_C_W_E));
    assert(lazy_exotic_calls == 0);
    JS_FreeValue(ctx, initializer);
    JS_FreeValue(ctx, object);

    /* Interrupts propagate unchanged and leave the original lazy record for
       a retry/redefinition after the embedder clears termination. */
    object = JS_NewObject(ctx);
    initializer = eval(ctx, "(function(){for(;;){}})");
    assert(1 == JS_DefineLazyDataProperty(ctx, object, atom, initializer, JS_PROP_C_W_E));
    JS_FreeValue(ctx, initializer);
    JS_SetInterruptHandler(rt, execution_interrupt, NULL);
    value = JS_GetProperty(ctx, object, atom);
    assert(JS_IsException(value));
    value = JS_GetException(ctx);
    assert(JS_IsUncatchableError(value));
    JS_FreeValue(ctx, value);
    JS_SetInterruptHandler(rt, NULL, NULL);
    assert(JS_IsLazyDataProperty(ctx, object, atom));
    int flags;
    assert(1 == JS_GetOwnPropertyFlags(ctx, &flags, object, atom));
    assert(flags == JS_PROP_C_W_E);
    assert(JS_IsLazyDataProperty(ctx, object, atom));
    JS_FreeValue(ctx, object);

    JS_FreeAtom(ctx, atom);
    JS_FreeValue(ctx, global);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

int main(int argc, char **argv)
{
    /* The fork diverges from several upstream API-test expectations. Allow
       independent regressions to run in CI without those other cases. */
    if (argc > 1) {
        assert(argc == 2);
        if (!strcmp(argv[1], "--is-executing-js"))
            is_executing_js();
        else {
            assert(!strcmp(argv[1], "--lazy-data-properties"));
            lazy_data_properties();
        }
        return 0;
    }
    cfunctions();
    sync_call();
    async_call();
    async_call_stack_overflow();
    raw_context_global_var();
    is_array();
    module_serde();
    module_unhandled_rejection();
    promise_mark_as_handled();
    promise_then();
    runtime_cstring_free();
    utf16_string();
    weak_map_gc_check();
    promise_hook();
    dump_memory_usage();
    new_errors();
    dom_exception_added_twice();
    large_allocation_accounting();
    backtrace_oom_current_exception();
    backtrace_oom_callsite_array();
    proxy_own_keys_huge_length();
    global_object_prototype();
    slice_string_tocstring();
    immutable_array_buffer();
    shared_array_buffer_growth();
    get_uint8array();
    new_symbol();
    bulk_free_macros();
    detach_array_buffer_free_once();
    typed_array_sort_index_overflow();
    transfer_external_array_buffer();
    resize_external_array_buffer();
    transfer_default_managed_array_buffer();
    get_class_name();
    object_from();
    add_intrinsic_bigint();
    new_typed_array();
    std_eval_interrupt_handler();
    private_symbols();
    is_executing_js();
    lazy_data_properties();
    return 0;
}
