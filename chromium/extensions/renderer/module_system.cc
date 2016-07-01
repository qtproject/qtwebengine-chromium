// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/module_system.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension.h"
#include "extensions/common/extensions_client.h"
#include "extensions/renderer/console.h"
#include "extensions/renderer/safe_builtins.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/renderer/v8_helpers.h"
#include "gin/modules/module_registry.h"
#include "third_party/WebKit/public/web/WebFrame.h"

namespace extensions {

using namespace v8_helpers;

namespace {

const char* kModuleSystem = "module_system";
const char* kModuleName = "module_name";
const char* kModuleField = "module_field";
const char* kModulesField = "modules";

// Logs an error for the calling context in preparation for potentially
// crashing the renderer, with some added metadata about the context:
//  - Its type (blessed, unblessed, etc).
//  - Whether it's valid.
//  - The extension ID, if one exists.
// Crashing won't happen in stable/beta releases, but is encouraged to happen
// in the less stable released to catch errors early.
void Fatal(ScriptContext* context, const std::string& message) {
  // Prepend some context metadata.
  std::string full_message = "(";
  if (!context->is_valid())
    full_message += "Invalid ";
  full_message += context->GetContextTypeDescription();
  full_message += " context";
  if (context->extension()) {
    full_message += " for ";
    full_message += context->extension()->id();
  }
  full_message += ") ";
  full_message += message;

  ExtensionsClient* client = ExtensionsClient::Get();
  if (client->ShouldSuppressFatalErrors()) {
    console::Error(context->GetRenderFrame(), full_message);
    client->RecordDidSuppressFatalError();
  } else {
    console::Fatal(context->GetRenderFrame(), full_message);
  }
}

void Warn(v8::Isolate* isolate, const std::string& message) {
  ScriptContext* script_context =
      ScriptContextSet::GetContextByV8Context(isolate->GetCurrentContext());
  console::Warn(script_context ? script_context->GetRenderFrame() : nullptr,
                message);
}

// Default exception handler which logs the exception.
class DefaultExceptionHandler : public ModuleSystem::ExceptionHandler {
 public:
  explicit DefaultExceptionHandler(ScriptContext* context)
      : ModuleSystem::ExceptionHandler(context) {}

  // Fatally dumps the debug info from |try_catch| to the console.
  // Make sure this is never used for exceptions that originate in external
  // code!
  void HandleUncaughtException(const v8::TryCatch& try_catch) override {
    v8::HandleScope handle_scope(context_->isolate());
    std::string stack_trace = "<stack trace unavailable>";
    v8::Local<v8::Value> v8_stack_trace;
    if (try_catch.StackTrace(context_->v8_context()).ToLocal(&v8_stack_trace)) {
      v8::String::Utf8Value stack_value(v8_stack_trace);
      if (*stack_value)
        stack_trace.assign(*stack_value, stack_value.length());
      else
        stack_trace = "<could not convert stack trace to string>";
    }
    Fatal(context_, CreateExceptionString(try_catch) + "{" + stack_trace + "}");
  }
};

// Sets a property on the "exports" object for bindings. Called by JS with
// exports.$set(<key>, <value>).
void SetExportsProperty(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Local<v8::Object> obj = args.This();
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsString());
  v8::Maybe<bool> result =
      obj->DefineOwnProperty(args.GetIsolate()->GetCurrentContext(),
                             args[0]->ToString(), args[1], v8::ReadOnly);
  if (!result.FromMaybe(false))
    LOG(ERROR) << "Failed to set private property on the export.";
}

}  // namespace

std::string ModuleSystem::ExceptionHandler::CreateExceptionString(
    const v8::TryCatch& try_catch) {
  v8::Local<v8::Message> message(try_catch.Message());
  if (message.IsEmpty()) {
    return "try_catch has no message";
  }

  std::string resource_name = "<unknown resource>";
  if (!message->GetScriptOrigin().ResourceName().IsEmpty()) {
    v8::String::Utf8Value resource_name_v8(
        message->GetScriptOrigin().ResourceName());
    resource_name.assign(*resource_name_v8, resource_name_v8.length());
  }

  std::string error_message = "<no error message>";
  if (!message->Get().IsEmpty()) {
    v8::String::Utf8Value error_message_v8(message->Get());
    error_message.assign(*error_message_v8, error_message_v8.length());
  }

  auto maybe = message->GetLineNumber(context_->v8_context());
  int line_number = maybe.IsJust() ? maybe.FromJust() : 0;
  return base::StringPrintf("%s:%d: %s",
                            resource_name.c_str(),
                            line_number,
                            error_message.c_str());
}

ModuleSystem::ModuleSystem(ScriptContext* context, SourceMap* source_map)
    : ObjectBackedNativeHandler(context),
      context_(context),
      source_map_(source_map),
      natives_enabled_(0),
      exception_handler_(new DefaultExceptionHandler(context)),
      weak_factory_(this) {
  RouteFunction(
      "require",
      base::Bind(&ModuleSystem::RequireForJs, base::Unretained(this)));
  RouteFunction(
      "requireNative",
      base::Bind(&ModuleSystem::RequireNative, base::Unretained(this)));
  RouteFunction(
      "requireAsync",
      base::Bind(&ModuleSystem::RequireAsync, base::Unretained(this)));
  RouteFunction("privates",
                base::Bind(&ModuleSystem::Private, base::Unretained(this)));

  v8::Local<v8::Object> global(context->v8_context()->Global());
  v8::Isolate* isolate = context->isolate();
  SetPrivate(global, kModulesField, v8::Object::New(isolate));
  SetPrivate(global, kModuleSystem, v8::External::New(isolate, this));

  gin::ModuleRegistry::From(context->v8_context())->AddObserver(this);
  if (context_->GetRenderFrame()) {
    context_->GetRenderFrame()->EnsureMojoBuiltinsAreAvailable(
        context->isolate(), context->v8_context());
  }
}

ModuleSystem::~ModuleSystem() {
}

void ModuleSystem::Invalidate() {
  // Clear the module system properties from the global context. It's polite,
  // and we use this as a signal in lazy handlers that we no longer exist.
  {
    v8::HandleScope scope(GetIsolate());
    v8::Local<v8::Object> global = context()->v8_context()->Global();
    DeletePrivate(global, kModulesField);
    DeletePrivate(global, kModuleSystem);
  }

  // Invalidate all active and clobbered NativeHandlers we own.
  for (const auto& handler : native_handler_map_)
    handler.second->Invalidate();
  for (const auto& clobbered_handler : clobbered_native_handlers_)
    clobbered_handler->Invalidate();

  ObjectBackedNativeHandler::Invalidate();
}

ModuleSystem::NativesEnabledScope::NativesEnabledScope(
    ModuleSystem* module_system)
    : module_system_(module_system) {
  module_system_->natives_enabled_++;
}

ModuleSystem::NativesEnabledScope::~NativesEnabledScope() {
  module_system_->natives_enabled_--;
  CHECK_GE(module_system_->natives_enabled_, 0);
}

void ModuleSystem::HandleException(const v8::TryCatch& try_catch) {
  exception_handler_->HandleUncaughtException(try_catch);
}

v8::MaybeLocal<v8::Object> ModuleSystem::Require(
    const std::string& module_name) {
  v8::Local<v8::String> v8_module_name;
  if (!ToV8String(GetIsolate(), module_name, &v8_module_name))
    return v8::MaybeLocal<v8::Object>();
  v8::EscapableHandleScope handle_scope(GetIsolate());
  v8::Local<v8::Value> value = RequireForJsInner(
      v8_module_name);
  if (value.IsEmpty() || !value->IsObject())
    return v8::MaybeLocal<v8::Object>();
  return handle_scope.Escape(value.As<v8::Object>());
}

void ModuleSystem::RequireForJs(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (!args[0]->IsString()) {
    NOTREACHED() << "require() called with a non-string argument";
    return;
  }
  v8::Local<v8::String> module_name = args[0].As<v8::String>();
  args.GetReturnValue().Set(RequireForJsInner(module_name));
}

v8::Local<v8::Value> ModuleSystem::RequireForJsInner(
    v8::Local<v8::String> module_name) {
  v8::EscapableHandleScope handle_scope(GetIsolate());
  v8::Local<v8::Context> v8_context = context()->v8_context();
  v8::Context::Scope context_scope(v8_context);

  v8::Local<v8::Object> global(context()->v8_context()->Global());

  // The module system might have been deleted. This can happen if a different
  // context keeps a reference to us, but our frame is destroyed (e.g.
  // background page keeps reference to chrome object in a closed popup).
  v8::Local<v8::Value> modules_value;
  if (!GetPrivate(global, kModulesField, &modules_value) ||
      modules_value->IsUndefined()) {
    Warn(GetIsolate(), "Extension view no longer exists");
    return v8::Undefined(GetIsolate());
  }

  v8::Local<v8::Object> modules(v8::Local<v8::Object>::Cast(modules_value));
  v8::Local<v8::Value> exports;
  if (!GetPrivateProperty(v8_context, modules, module_name, &exports) ||
      !exports->IsUndefined())
    return handle_scope.Escape(exports);

  exports = LoadModule(*v8::String::Utf8Value(module_name));
  SetPrivateProperty(v8_context, modules, module_name, exports);
  return handle_scope.Escape(exports);
}

v8::Local<v8::Value> ModuleSystem::CallModuleMethod(
    const std::string& module_name,
    const std::string& method_name) {
  v8::EscapableHandleScope handle_scope(GetIsolate());
  v8::Local<v8::Value> no_args;
  return handle_scope.Escape(
      CallModuleMethod(module_name, method_name, 0, &no_args));
}

v8::Local<v8::Value> ModuleSystem::CallModuleMethod(
    const std::string& module_name,
    const std::string& method_name,
    std::vector<v8::Local<v8::Value>>* args) {
  return CallModuleMethod(module_name, method_name, args->size(), args->data());
}

v8::Local<v8::Value> ModuleSystem::CallModuleMethod(
    const std::string& module_name,
    const std::string& method_name,
    int argc,
    v8::Local<v8::Value> argv[]) {
  TRACE_EVENT2("v8",
               "v8.callModuleMethod",
               "module_name",
               module_name,
               "method_name",
               method_name);

  v8::EscapableHandleScope handle_scope(GetIsolate());
  v8::Local<v8::Context> v8_context = context()->v8_context();
  v8::Context::Scope context_scope(v8_context);

  v8::Local<v8::String> v8_module_name;
  v8::Local<v8::String> v8_method_name;
  if (!ToV8String(GetIsolate(), module_name.c_str(), &v8_module_name) ||
      !ToV8String(GetIsolate(), method_name.c_str(), &v8_method_name)) {
    return handle_scope.Escape(v8::Undefined(GetIsolate()));
  }

  v8::Local<v8::Value> module;
  {
    NativesEnabledScope natives_enabled(this);
    module = RequireForJsInner(v8_module_name);
  }

  if (module.IsEmpty() || !module->IsObject()) {
    Fatal(context_,
          "Failed to get module " + module_name + " to call " + method_name);
    return handle_scope.Escape(v8::Undefined(GetIsolate()));
  }

  v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(module);
  v8::Local<v8::Value> value;
  if (!GetProperty(v8_context, object, v8_method_name, &value) ||
      !value->IsFunction()) {
    Fatal(context_, module_name + "." + method_name + " is not a function");
    return handle_scope.Escape(v8::Undefined(GetIsolate()));
  }

  v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast(value);
  v8::Local<v8::Value> result;
  {
    v8::TryCatch try_catch(GetIsolate());
    try_catch.SetCaptureMessage(true);
    result = context_->CallFunction(func, argc, argv);
    if (try_catch.HasCaught()) {
      HandleException(try_catch);
      result = v8::Undefined(GetIsolate());
    }
  }
  return handle_scope.Escape(result);
}

void ModuleSystem::RegisterNativeHandler(
    const std::string& name,
    scoped_ptr<NativeHandler> native_handler) {
  ClobberExistingNativeHandler(name);
  native_handler_map_[name] = std::move(native_handler);
}

void ModuleSystem::OverrideNativeHandlerForTest(const std::string& name) {
  ClobberExistingNativeHandler(name);
  overridden_native_handlers_.insert(name);
}

void ModuleSystem::RunString(const std::string& code, const std::string& name) {
  v8::HandleScope handle_scope(GetIsolate());
  v8::Local<v8::String> v8_code;
  v8::Local<v8::String> v8_name;
  if (!ToV8String(GetIsolate(), code.c_str(), &v8_code) ||
      !ToV8String(GetIsolate(), name.c_str(), &v8_name)) {
    Warn(GetIsolate(), "Too long code or name.");
    return;
  }
  RunString(v8_code, v8_name);
}

// static
void ModuleSystem::NativeLazyFieldGetter(
    v8::Local<v8::Name> property,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  LazyFieldGetterInner(property.As<v8::String>(), info,
                       &ModuleSystem::RequireNativeFromString);
}

// static
void ModuleSystem::LazyFieldGetter(
    v8::Local<v8::Name> property,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  LazyFieldGetterInner(property.As<v8::String>(), info, &ModuleSystem::Require);
}

// static
void ModuleSystem::LazyFieldGetterInner(
    v8::Local<v8::String> property,
    const v8::PropertyCallbackInfo<v8::Value>& info,
    RequireFunction require_function) {
  CHECK(!info.Data().IsEmpty());
  CHECK(info.Data()->IsObject());
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> parameters = v8::Local<v8::Object>::Cast(info.Data());
  // This context should be the same as context()->v8_context().
  v8::Local<v8::Context> context = parameters->CreationContext();
  v8::Local<v8::Object> global(context->Global());
  v8::Local<v8::Value> module_system_value;
  if (!GetPrivate(context, global, kModuleSystem, &module_system_value) ||
      !module_system_value->IsExternal()) {
    // ModuleSystem has been deleted.
    // TODO(kalman): See comment in header file.
    Warn(isolate,
         "Module system has been deleted, does extension view exist?");
    return;
  }

  ModuleSystem* module_system = static_cast<ModuleSystem*>(
      v8::Local<v8::External>::Cast(module_system_value)->Value());

  v8::Local<v8::Value> v8_module_name;
  if (!GetPrivateProperty(context, parameters, kModuleName, &v8_module_name)) {
    Warn(isolate, "Cannot find module.");
    return;
  }
  std::string name = *v8::String::Utf8Value(v8_module_name);

  // Switch to our v8 context because we need functions created while running
  // the require()d module to belong to our context, not the current one.
  v8::Context::Scope context_scope(context);
  NativesEnabledScope natives_enabled_scope(module_system);

  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Value> module_value;
  if (!(module_system->*require_function)(name).ToLocal(&module_value)) {
    module_system->HandleException(try_catch);
    return;
  }

  v8::Local<v8::Object> module = v8::Local<v8::Object>::Cast(module_value);
  v8::Local<v8::Value> field_value;
  if (!GetPrivateProperty(context, parameters, kModuleField, &field_value)) {
    module_system->HandleException(try_catch);
    return;
  }
  v8::Local<v8::String> field;
  if (!field_value->ToString(context).ToLocal(&field)) {
    module_system->HandleException(try_catch);
    return;
  }

  if (!IsTrue(module->Has(context, field))) {
    std::string field_str = *v8::String::Utf8Value(field);
    Fatal(module_system->context_,
          "Lazy require of " + name + "." + field_str + " did not set the " +
              field_str + " field");
    return;
  }

  v8::Local<v8::Value> new_field;
  if (!GetProperty(context, module, field, &new_field)) {
    module_system->HandleException(try_catch);
    return;
  }

  // Ok for it to be undefined, among other things it's how bindings signify
  // that the extension doesn't have permission to use them.
  CHECK(!new_field.IsEmpty());

  // Delete the getter and set this field to |new_field| so the same object is
  // returned every time a certain API is accessed.
  v8::Local<v8::Value> val = info.This();
  if (val->IsObject()) {
    v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(val);
    object->Delete(context, property);
    SetProperty(context, object, property, new_field);
  } else {
    NOTREACHED();
  }
  info.GetReturnValue().Set(new_field);
}

void ModuleSystem::SetLazyField(v8::Local<v8::Object> object,
                                const std::string& field,
                                const std::string& module_name,
                                const std::string& module_field) {
  SetLazyField(
      object, field, module_name, module_field, &ModuleSystem::LazyFieldGetter);
}

void ModuleSystem::SetLazyField(v8::Local<v8::Object> object,
                                const std::string& field,
                                const std::string& module_name,
                                const std::string& module_field,
                                v8::AccessorNameGetterCallback getter) {
  CHECK(field.size() < v8::String::kMaxLength);
  CHECK(module_name.size() < v8::String::kMaxLength);
  CHECK(module_field.size() < v8::String::kMaxLength);
  v8::HandleScope handle_scope(GetIsolate());
  v8::Local<v8::Object> parameters = v8::Object::New(GetIsolate());
  v8::Local<v8::Context> context = context_->v8_context();
  SetPrivateProperty(context, parameters, kModuleName,
              ToV8StringUnsafe(GetIsolate(), module_name.c_str()));
  SetPrivateProperty(context, parameters, kModuleField,
              ToV8StringUnsafe(GetIsolate(), module_field.c_str()));
  auto maybe = object->SetAccessor(
      context, ToV8StringUnsafe(GetIsolate(), field.c_str()), getter, NULL,
      parameters);
  CHECK(IsTrue(maybe));
}

void ModuleSystem::SetNativeLazyField(v8::Local<v8::Object> object,
                                      const std::string& field,
                                      const std::string& module_name,
                                      const std::string& module_field) {
  SetLazyField(object,
               field,
               module_name,
               module_field,
               &ModuleSystem::NativeLazyFieldGetter);
}

v8::Local<v8::Value> ModuleSystem::RunString(v8::Local<v8::String> code,
                                             v8::Local<v8::String> name) {
  return context_->RunScript(
      name, code, base::Bind(&ExceptionHandler::HandleUncaughtException,
                             base::Unretained(exception_handler_.get())));
}

v8::Local<v8::Value> ModuleSystem::GetSource(const std::string& module_name) {
  v8::EscapableHandleScope handle_scope(GetIsolate());
  if (!source_map_->Contains(module_name))
    return v8::Undefined(GetIsolate());
  return handle_scope.Escape(
      v8::Local<v8::Value>(source_map_->GetSource(GetIsolate(), module_name)));
}

void ModuleSystem::RequireNative(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  std::string native_name = *v8::String::Utf8Value(args[0]);
  v8::Local<v8::Object> object;
  if (RequireNativeFromString(native_name).ToLocal(&object))
    args.GetReturnValue().Set(object);
}

v8::MaybeLocal<v8::Object> ModuleSystem::RequireNativeFromString(
    const std::string& native_name) {
  if (natives_enabled_ == 0) {
    // HACK: if in test throw exception so that we can test the natives-disabled
    // logic; however, under normal circumstances, this is programmer error so
    // we could crash.
    if (exception_handler_) {
      GetIsolate()->ThrowException(
          ToV8StringUnsafe(GetIsolate(), "Natives disabled"));
      return v8::MaybeLocal<v8::Object>();
    }
    Fatal(context_, "Natives disabled for requireNative(" + native_name + ")");
    return v8::MaybeLocal<v8::Object>();
  }

  if (overridden_native_handlers_.count(native_name) > 0u) {
    v8::Local<v8::Value> value = RequireForJsInner(
        ToV8StringUnsafe(GetIsolate(), native_name.c_str()));
    if (value.IsEmpty() || !value->IsObject())
      return v8::MaybeLocal<v8::Object>();
    return value.As<v8::Object>();
  }

  NativeHandlerMap::iterator i = native_handler_map_.find(native_name);
  if (i == native_handler_map_.end()) {
    Fatal(context_,
          "Couldn't find native for requireNative(" + native_name + ")");
    return v8::MaybeLocal<v8::Object>();
  }
  return i->second->NewInstance();
}

void ModuleSystem::RequireAsync(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  std::string module_name = *v8::String::Utf8Value(args[0]);
  v8::Local<v8::Context> v8_context = context_->v8_context();
  v8::Local<v8::Promise::Resolver> resolver(
      v8::Promise::Resolver::New(v8_context).ToLocalChecked());
  args.GetReturnValue().Set(resolver->GetPromise());
  scoped_ptr<v8::Global<v8::Promise::Resolver>> global_resolver(
      new v8::Global<v8::Promise::Resolver>(GetIsolate(), resolver));
  gin::ModuleRegistry* module_registry =
      gin::ModuleRegistry::From(v8_context);
  if (!module_registry) {
    Warn(GetIsolate(), "Extension view no longer exists");
    resolver->Reject(v8_context, v8::Exception::Error(ToV8StringUnsafe(
        GetIsolate(), "Extension view no longer exists")));
    return;
  }
  module_registry->LoadModule(
      GetIsolate(), module_name,
      base::Bind(&ModuleSystem::OnModuleLoaded, weak_factory_.GetWeakPtr(),
                 base::Passed(&global_resolver)));
  if (module_registry->available_modules().count(module_name) == 0)
    LoadModule(module_name);
}

v8::Local<v8::String> ModuleSystem::WrapSource(v8::Local<v8::String> source) {
  v8::EscapableHandleScope handle_scope(GetIsolate());
  // Keep in order with the arguments in RequireForJsInner.
  v8::Local<v8::String> left = ToV8StringUnsafe(
      GetIsolate(),
      "(function(define, require, requireNative, requireAsync, exports, "
      "console, privates,"
      "$Array, $Function, $JSON, $Object, $RegExp, $String, $Error) {"
      "'use strict';");
  v8::Local<v8::String> right = ToV8StringUnsafe(GetIsolate(), "\n})");
  return handle_scope.Escape(v8::Local<v8::String>(
      v8::String::Concat(left, v8::String::Concat(source, right))));
}

void ModuleSystem::Private(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  if (!args[0]->IsObject() || args[0]->IsNull()) {
    GetIsolate()->ThrowException(
        v8::Exception::TypeError(ToV8StringUnsafe(GetIsolate(),
            args[0]->IsUndefined()
                ? "Method called without a valid receiver (this). "
                  "Did you forget to call .bind()?"
                : "Invalid invocation: receiver is not an object!")));
    return;
  }
  v8::Local<v8::Object> obj = args[0].As<v8::Object>();
  v8::Local<v8::Value> privates;
  if (!GetPrivate(obj, "privates", &privates) || !privates->IsObject()) {
    privates = v8::Object::New(args.GetIsolate());
    if (privates.IsEmpty()) {
      GetIsolate()->ThrowException(
          ToV8StringUnsafe(GetIsolate(), "Failed to create privates"));
      return;
    }
    v8::Maybe<bool> maybe =
        privates.As<v8::Object>()->SetPrototype(context()->v8_context(),
                                                v8::Null(args.GetIsolate()));
    CHECK(maybe.IsJust() && maybe.FromJust());
    SetPrivate(obj, "privates", privates);
  }
  args.GetReturnValue().Set(privates);
}

v8::Local<v8::Value> ModuleSystem::LoadModule(const std::string& module_name) {
  v8::EscapableHandleScope handle_scope(GetIsolate());
  v8::Local<v8::Context> v8_context = context()->v8_context();
  v8::Context::Scope context_scope(v8_context);

  v8::Local<v8::Value> source(GetSource(module_name));
  if (source.IsEmpty() || source->IsUndefined()) {
    Fatal(context_, "No source for require(" + module_name + ")");
    return v8::Undefined(GetIsolate());
  }
  v8::Local<v8::String> wrapped_source(
      WrapSource(v8::Local<v8::String>::Cast(source)));
  v8::Local<v8::String> v8_module_name;
  if (!ToV8String(GetIsolate(), module_name.c_str(), &v8_module_name)) {
    NOTREACHED() << "module_name is too long";
    return v8::Undefined(GetIsolate());
  }
  // Modules are wrapped in (function(){...}) so they always return functions.
  v8::Local<v8::Value> func_as_value =
      RunString(wrapped_source, v8_module_name);
  if (func_as_value.IsEmpty() || func_as_value->IsUndefined()) {
    Fatal(context_, "Bad source for require(" + module_name + ")");
    return v8::Undefined(GetIsolate());
  }

  v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast(func_as_value);

  v8::Local<v8::Object> define_object = v8::Object::New(GetIsolate());
  gin::ModuleRegistry::InstallGlobals(GetIsolate(), define_object);

  v8::Local<v8::Object> exports = v8::Object::New(GetIsolate());

  v8::Local<v8::FunctionTemplate> tmpl = v8::FunctionTemplate::New(
      GetIsolate(),
      &SetExportsProperty);
  v8::Local<v8::String> v8_key;
  if (!v8_helpers::ToV8String(GetIsolate(), "$set", &v8_key)) {
    NOTREACHED();
    return v8::Undefined(GetIsolate());
  }

  v8::Local<v8::Function> function;
  if (!tmpl->GetFunction(v8_context).ToLocal(&function)) {
    NOTREACHED();
    return v8::Undefined(GetIsolate());
  }

  exports->DefineOwnProperty(v8_context, v8_key, function, v8::ReadOnly)
      .FromJust();

  v8::Local<v8::Object> natives(NewInstance());
  CHECK(!natives.IsEmpty());  // this can fail if v8 has issues

  // These must match the argument order in WrapSource.
  v8::Local<v8::Value> args[] = {
      // AMD.
      GetPropertyUnsafe(v8_context, define_object, "define"),
      // CommonJS.
      GetPropertyUnsafe(v8_context, natives, "require",
                        v8::NewStringType::kInternalized),
      GetPropertyUnsafe(v8_context, natives, "requireNative",
                        v8::NewStringType::kInternalized),
      GetPropertyUnsafe(v8_context, natives, "requireAsync",
                        v8::NewStringType::kInternalized),
      exports,
      // Libraries that we magically expose to every module.
      console::AsV8Object(GetIsolate()),
      GetPropertyUnsafe(v8_context, natives, "privates",
                        v8::NewStringType::kInternalized),
      // Each safe builtin. Keep in order with the arguments in WrapSource.
      context_->safe_builtins()->GetArray(),
      context_->safe_builtins()->GetFunction(),
      context_->safe_builtins()->GetJSON(),
      context_->safe_builtins()->GetObjekt(),
      context_->safe_builtins()->GetRegExp(),
      context_->safe_builtins()->GetString(),
      context_->safe_builtins()->GetError(),
  };
  {
    v8::TryCatch try_catch(GetIsolate());
    try_catch.SetCaptureMessage(true);
    context_->CallFunction(func, arraysize(args), args);
    if (try_catch.HasCaught()) {
      HandleException(try_catch);
      return v8::Undefined(GetIsolate());
    }
  }
  return handle_scope.Escape(exports);
}

void ModuleSystem::OnDidAddPendingModule(
    const std::string& id,
    const std::vector<std::string>& dependencies) {
  bool module_system_managed = source_map_->Contains(id);

  gin::ModuleRegistry* registry =
      gin::ModuleRegistry::From(context_->v8_context());
  DCHECK(registry);
  for (const auto& dependency : dependencies) {
    // If a dependency is not available, and either the module or this
    // dependency is managed by ModuleSystem, attempt to load it. Other
    // gin::ModuleRegistry users (WebUI and users of the mojoPrivate API) are
    // responsible for loading their module dependencies when required.
    if (registry->available_modules().count(dependency) == 0 &&
        (module_system_managed || source_map_->Contains(dependency))) {
      LoadModule(dependency);
    }
  }
  registry->AttemptToLoadMoreModules(GetIsolate());
}

void ModuleSystem::OnModuleLoaded(
    scoped_ptr<v8::Global<v8::Promise::Resolver>> resolver,
    v8::Local<v8::Value> value) {
  if (!is_valid())
    return;
  v8::HandleScope handle_scope(GetIsolate());
  v8::Local<v8::Promise::Resolver> resolver_local(
      v8::Local<v8::Promise::Resolver>::New(GetIsolate(), *resolver));
  resolver_local->Resolve(context()->v8_context(), value);
}

void ModuleSystem::ClobberExistingNativeHandler(const std::string& name) {
  NativeHandlerMap::iterator existing_handler = native_handler_map_.find(name);
  if (existing_handler != native_handler_map_.end()) {
    clobbered_native_handlers_.push_back(std::move(existing_handler->second));
    native_handler_map_.erase(existing_handler);
  }
}

}  // namespace extensions
