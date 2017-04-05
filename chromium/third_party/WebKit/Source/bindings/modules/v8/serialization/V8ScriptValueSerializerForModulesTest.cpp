// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/modules/v8/serialization/V8ScriptValueSerializerForModules.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ToV8.h"
#include "bindings/core/v8/V8ArrayBuffer.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8DOMException.h"
#include "bindings/modules/v8/V8CryptoKey.h"
#include "bindings/modules/v8/V8DOMFileSystem.h"
#include "bindings/modules/v8/V8RTCCertificate.h"
#include "bindings/modules/v8/serialization/V8ScriptValueDeserializerForModules.h"
#include "core/dom/DOMArrayBuffer.h"
#include "modules/crypto/CryptoResultImpl.h"
#include "modules/filesystem/DOMFileSystem.h"
#include "modules/peerconnection/RTCCertificate.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCryptoAlgorithmParams.h"
#include "public/platform/WebRTCCertificateGenerator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::UnorderedElementsAre;

namespace blink {
namespace {

class ScopedEnableV8BasedStructuredClone {
 public:
  ScopedEnableV8BasedStructuredClone()
      : m_wasEnabled(RuntimeEnabledFeatures::v8BasedStructuredCloneEnabled()) {
    RuntimeEnabledFeatures::setV8BasedStructuredCloneEnabled(true);
  }
  ~ScopedEnableV8BasedStructuredClone() {
    RuntimeEnabledFeatures::setV8BasedStructuredCloneEnabled(m_wasEnabled);
  }

 private:
  bool m_wasEnabled;
};

RefPtr<SerializedScriptValue> serializedValue(const Vector<uint8_t>& bytes) {
  // TODO(jbroman): Fix this once SerializedScriptValue can take bytes without
  // endianness swapping.
  DCHECK_EQ(bytes.size() % 2, 0u);
  return SerializedScriptValue::create(
      String(reinterpret_cast<const UChar*>(&bytes[0]), bytes.size() / 2));
}

v8::Local<v8::Value> roundTrip(v8::Local<v8::Value> value,
                               V8TestingScope& scope) {
  RefPtr<ScriptState> scriptState = scope.getScriptState();
  ExceptionState& exceptionState = scope.getExceptionState();
  RefPtr<SerializedScriptValue> serializedScriptValue =
      V8ScriptValueSerializerForModules(scriptState)
          .serialize(value, nullptr, exceptionState);
  DCHECK_EQ(!serializedScriptValue, exceptionState.hadException());
  EXPECT_TRUE(serializedScriptValue);
  if (!serializedScriptValue)
    return v8::Local<v8::Value>();
  return V8ScriptValueDeserializerForModules(scriptState, serializedScriptValue)
      .deserialize();
}

// Checks for a DOM exception, including a rethrown one.
::testing::AssertionResult hadDOMException(const StringView& name,
                                           ScriptState* scriptState,
                                           ExceptionState& exceptionState) {
  if (!exceptionState.hadException())
    return ::testing::AssertionFailure() << "no exception thrown";
  DOMException* domException = V8DOMException::toImplWithTypeCheck(
      scriptState->isolate(), exceptionState.getException());
  if (!domException) {
    return ::testing::AssertionFailure()
           << "exception thrown was not a DOMException";
  }
  if (domException->name() != name)
    return ::testing::AssertionFailure() << "was " << domException->name();
  return ::testing::AssertionSuccess();
}

static const char kEcdsaPrivateKey[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQghHwQ1xYtCoEhFk7r\n"
    "92u3ozy/MFR4I+9FiN8RYv5J96GhRANCAATLfi7OZLD9sIe5UMfMQnHQgAFaQD8h\n"
    "/cy6tB8wXZcixp7bZDp5t0GCDHqAUZT3Sa/NHaCelmmgPp3zW3lszXKP\n"
    "-----END PRIVATE KEY-----\n";

static const char kEcdsaCertificate[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBFjCBvaADAgECAgkApnGS+DzNWkUwCgYIKoZIzj0EAwIwETEPMA0GA1UEAwwG\n"
    "V2ViUlRDMB4XDTE2MDkxNTE4MDcxMloXDTE2MTAxNjE4MDcxMlowETEPMA0GA1UE\n"
    "AwwGV2ViUlRDMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEy34uzmSw/bCHuVDH\n"
    "zEJx0IABWkA/If3MurQfMF2XIsae22Q6ebdBggx6gFGU90mvzR2gnpZpoD6d81t5\n"
    "bM1yjzAKBggqhkjOPQQDAgNIADBFAiBcTOyiexG0QHa5WhJuGtY6FhVZ5GyBMW+7\n"
    "LkH2QmxICwIhAJCujozN3gjIu7NMxSXuTqueuVz58SefCMA7/vj1TgfV\n"
    "-----END CERTIFICATE-----\n";

static const uint8_t kEcdsaCertificateEncoded[] = {
    0xff, 0x09, 0x3f, 0x00, 0x6b, 0xf1, 0x01, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d,
    0x42, 0x45, 0x47, 0x49, 0x4e, 0x20, 0x50, 0x52, 0x49, 0x56, 0x41, 0x54,
    0x45, 0x20, 0x4b, 0x45, 0x59, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x0a, 0x4d,
    0x49, 0x47, 0x48, 0x41, 0x67, 0x45, 0x41, 0x4d, 0x42, 0x4d, 0x47, 0x42,
    0x79, 0x71, 0x47, 0x53, 0x4d, 0x34, 0x39, 0x41, 0x67, 0x45, 0x47, 0x43,
    0x43, 0x71, 0x47, 0x53, 0x4d, 0x34, 0x39, 0x41, 0x77, 0x45, 0x48, 0x42,
    0x47, 0x30, 0x77, 0x61, 0x77, 0x49, 0x42, 0x41, 0x51, 0x51, 0x67, 0x68,
    0x48, 0x77, 0x51, 0x31, 0x78, 0x59, 0x74, 0x43, 0x6f, 0x45, 0x68, 0x46,
    0x6b, 0x37, 0x72, 0x0a, 0x39, 0x32, 0x75, 0x33, 0x6f, 0x7a, 0x79, 0x2f,
    0x4d, 0x46, 0x52, 0x34, 0x49, 0x2b, 0x39, 0x46, 0x69, 0x4e, 0x38, 0x52,
    0x59, 0x76, 0x35, 0x4a, 0x39, 0x36, 0x47, 0x68, 0x52, 0x41, 0x4e, 0x43,
    0x41, 0x41, 0x54, 0x4c, 0x66, 0x69, 0x37, 0x4f, 0x5a, 0x4c, 0x44, 0x39,
    0x73, 0x49, 0x65, 0x35, 0x55, 0x4d, 0x66, 0x4d, 0x51, 0x6e, 0x48, 0x51,
    0x67, 0x41, 0x46, 0x61, 0x51, 0x44, 0x38, 0x68, 0x0a, 0x2f, 0x63, 0x79,
    0x36, 0x74, 0x42, 0x38, 0x77, 0x58, 0x5a, 0x63, 0x69, 0x78, 0x70, 0x37,
    0x62, 0x5a, 0x44, 0x70, 0x35, 0x74, 0x30, 0x47, 0x43, 0x44, 0x48, 0x71,
    0x41, 0x55, 0x5a, 0x54, 0x33, 0x53, 0x61, 0x2f, 0x4e, 0x48, 0x61, 0x43,
    0x65, 0x6c, 0x6d, 0x6d, 0x67, 0x50, 0x70, 0x33, 0x7a, 0x57, 0x33, 0x6c,
    0x73, 0x7a, 0x58, 0x4b, 0x50, 0x0a, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x45,
    0x4e, 0x44, 0x20, 0x50, 0x52, 0x49, 0x56, 0x41, 0x54, 0x45, 0x20, 0x4b,
    0x45, 0x59, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x0a, 0xb4, 0x03, 0x2d, 0x2d,
    0x2d, 0x2d, 0x2d, 0x42, 0x45, 0x47, 0x49, 0x4e, 0x20, 0x43, 0x45, 0x52,
    0x54, 0x49, 0x46, 0x49, 0x43, 0x41, 0x54, 0x45, 0x2d, 0x2d, 0x2d, 0x2d,
    0x2d, 0x0a, 0x4d, 0x49, 0x49, 0x42, 0x46, 0x6a, 0x43, 0x42, 0x76, 0x61,
    0x41, 0x44, 0x41, 0x67, 0x45, 0x43, 0x41, 0x67, 0x6b, 0x41, 0x70, 0x6e,
    0x47, 0x53, 0x2b, 0x44, 0x7a, 0x4e, 0x57, 0x6b, 0x55, 0x77, 0x43, 0x67,
    0x59, 0x49, 0x4b, 0x6f, 0x5a, 0x49, 0x7a, 0x6a, 0x30, 0x45, 0x41, 0x77,
    0x49, 0x77, 0x45, 0x54, 0x45, 0x50, 0x4d, 0x41, 0x30, 0x47, 0x41, 0x31,
    0x55, 0x45, 0x41, 0x77, 0x77, 0x47, 0x0a, 0x56, 0x32, 0x56, 0x69, 0x55,
    0x6c, 0x52, 0x44, 0x4d, 0x42, 0x34, 0x58, 0x44, 0x54, 0x45, 0x32, 0x4d,
    0x44, 0x6b, 0x78, 0x4e, 0x54, 0x45, 0x34, 0x4d, 0x44, 0x63, 0x78, 0x4d,
    0x6c, 0x6f, 0x58, 0x44, 0x54, 0x45, 0x32, 0x4d, 0x54, 0x41, 0x78, 0x4e,
    0x6a, 0x45, 0x34, 0x4d, 0x44, 0x63, 0x78, 0x4d, 0x6c, 0x6f, 0x77, 0x45,
    0x54, 0x45, 0x50, 0x4d, 0x41, 0x30, 0x47, 0x41, 0x31, 0x55, 0x45, 0x0a,
    0x41, 0x77, 0x77, 0x47, 0x56, 0x32, 0x56, 0x69, 0x55, 0x6c, 0x52, 0x44,
    0x4d, 0x46, 0x6b, 0x77, 0x45, 0x77, 0x59, 0x48, 0x4b, 0x6f, 0x5a, 0x49,
    0x7a, 0x6a, 0x30, 0x43, 0x41, 0x51, 0x59, 0x49, 0x4b, 0x6f, 0x5a, 0x49,
    0x7a, 0x6a, 0x30, 0x44, 0x41, 0x51, 0x63, 0x44, 0x51, 0x67, 0x41, 0x45,
    0x79, 0x33, 0x34, 0x75, 0x7a, 0x6d, 0x53, 0x77, 0x2f, 0x62, 0x43, 0x48,
    0x75, 0x56, 0x44, 0x48, 0x0a, 0x7a, 0x45, 0x4a, 0x78, 0x30, 0x49, 0x41,
    0x42, 0x57, 0x6b, 0x41, 0x2f, 0x49, 0x66, 0x33, 0x4d, 0x75, 0x72, 0x51,
    0x66, 0x4d, 0x46, 0x32, 0x58, 0x49, 0x73, 0x61, 0x65, 0x32, 0x32, 0x51,
    0x36, 0x65, 0x62, 0x64, 0x42, 0x67, 0x67, 0x78, 0x36, 0x67, 0x46, 0x47,
    0x55, 0x39, 0x30, 0x6d, 0x76, 0x7a, 0x52, 0x32, 0x67, 0x6e, 0x70, 0x5a,
    0x70, 0x6f, 0x44, 0x36, 0x64, 0x38, 0x31, 0x74, 0x35, 0x0a, 0x62, 0x4d,
    0x31, 0x79, 0x6a, 0x7a, 0x41, 0x4b, 0x42, 0x67, 0x67, 0x71, 0x68, 0x6b,
    0x6a, 0x4f, 0x50, 0x51, 0x51, 0x44, 0x41, 0x67, 0x4e, 0x49, 0x41, 0x44,
    0x42, 0x46, 0x41, 0x69, 0x42, 0x63, 0x54, 0x4f, 0x79, 0x69, 0x65, 0x78,
    0x47, 0x30, 0x51, 0x48, 0x61, 0x35, 0x57, 0x68, 0x4a, 0x75, 0x47, 0x74,
    0x59, 0x36, 0x46, 0x68, 0x56, 0x5a, 0x35, 0x47, 0x79, 0x42, 0x4d, 0x57,
    0x2b, 0x37, 0x0a, 0x4c, 0x6b, 0x48, 0x32, 0x51, 0x6d, 0x78, 0x49, 0x43,
    0x77, 0x49, 0x68, 0x41, 0x4a, 0x43, 0x75, 0x6a, 0x6f, 0x7a, 0x4e, 0x33,
    0x67, 0x6a, 0x49, 0x75, 0x37, 0x4e, 0x4d, 0x78, 0x53, 0x58, 0x75, 0x54,
    0x71, 0x75, 0x65, 0x75, 0x56, 0x7a, 0x35, 0x38, 0x53, 0x65, 0x66, 0x43,
    0x4d, 0x41, 0x37, 0x2f, 0x76, 0x6a, 0x31, 0x54, 0x67, 0x66, 0x56, 0x0a,
    0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x45, 0x4e, 0x44, 0x20, 0x43, 0x45, 0x52,
    0x54, 0x49, 0x46, 0x49, 0x43, 0x41, 0x54, 0x45, 0x2d, 0x2d, 0x2d, 0x2d,
    0x2d, 0x0a};

TEST(V8ScriptValueSerializerForModulesTest, RoundTripRTCCertificate) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;

  // Make a certificate with the existing key above.
  std::unique_ptr<WebRTCCertificateGenerator> certificateGenerator(
      Platform::current()->createRTCCertificateGenerator());
  std::unique_ptr<WebRTCCertificate> webCertificate =
      certificateGenerator->fromPEM(
          WebString::fromUTF8(kEcdsaPrivateKey, sizeof(kEcdsaPrivateKey)),
          WebString::fromUTF8(kEcdsaCertificate, sizeof(kEcdsaCertificate)));
  ASSERT_TRUE(webCertificate);
  RTCCertificate* certificate = new RTCCertificate(std::move(webCertificate));

  // Round trip test.
  v8::Local<v8::Value> wrapper =
      ToV8(certificate, scope.context()->Global(), scope.isolate());
  v8::Local<v8::Value> result = roundTrip(wrapper, scope);
  ASSERT_TRUE(V8RTCCertificate::hasInstance(result, scope.isolate()));
  RTCCertificate* newCertificate =
      V8RTCCertificate::toImpl(result.As<v8::Object>());
  WebRTCCertificatePEM pem = newCertificate->certificate().toPEM();
  EXPECT_EQ(kEcdsaPrivateKey, pem.privateKey());
  EXPECT_EQ(kEcdsaCertificate, pem.certificate());
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeRTCCertificate) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;

  // This is encoded data generated from Chromium (around M55).
  ScriptState* scriptState = scope.getScriptState();
  Vector<uint8_t> encodedData;
  encodedData.append(kEcdsaCertificateEncoded,
                     sizeof(kEcdsaCertificateEncoded));
  RefPtr<SerializedScriptValue> input = serializedValue(encodedData);

  // Decode test.
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializerForModules(scriptState, input).deserialize();
  ASSERT_TRUE(V8RTCCertificate::hasInstance(result, scope.isolate()));
  RTCCertificate* newCertificate =
      V8RTCCertificate::toImpl(result.As<v8::Object>());
  WebRTCCertificatePEM pem = newCertificate->certificate().toPEM();
  EXPECT_EQ(kEcdsaPrivateKey, pem.privateKey());
  EXPECT_EQ(kEcdsaCertificate, pem.certificate());
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeInvalidRTCCertificate) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;

  // This is valid, except that "private" is not a valid private key PEM and
  // "certificate" is not a valid certificate PEM. This checks what happens if
  // these fail validation inside WebRTC.
  ScriptState* scriptState = scope.getScriptState();
  RefPtr<SerializedScriptValue> input = serializedValue(
      {0xff, 0x09, 0x3f, 0x00, 0x6b, 0x07, 'p', 'r', 'i', 'v', 'a', 't', 'e',
       0x0b, 'c',  'e',  'r',  't',  'i',  'f', 'i', 'c', 'a', 't', 'e', 0x00});

  // Decode test.
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializerForModules(scriptState, input).deserialize();
  EXPECT_TRUE(result->IsNull());
}

// A bunch of voodoo which allows the asynchronous WebCrypto operations to be
// called synchronously, with the resulting JavaScript values extracted.

using CryptoKeyPair = std::pair<CryptoKey*, CryptoKey*>;

template <typename T>
T convertCryptoResult(const ScriptValue&);
template <>
CryptoKey* convertCryptoResult<CryptoKey*>(const ScriptValue& value) {
  return V8CryptoKey::toImplWithTypeCheck(value.isolate(), value.v8Value());
}
template <>
CryptoKeyPair convertCryptoResult<CryptoKeyPair>(const ScriptValue& value) {
  NonThrowableExceptionState exceptionState;
  Dictionary dictionary(value.isolate(), value.v8Value(), exceptionState);
  v8::Local<v8::Value> privateKey, publicKey;
  EXPECT_TRUE(dictionary.get("publicKey", publicKey));
  EXPECT_TRUE(dictionary.get("privateKey", privateKey));
  return std::make_pair(
      V8CryptoKey::toImplWithTypeCheck(value.isolate(), publicKey),
      V8CryptoKey::toImplWithTypeCheck(value.isolate(), privateKey));
}
template <>
DOMException* convertCryptoResult<DOMException*>(const ScriptValue& value) {
  return V8DOMException::toImplWithTypeCheck(value.isolate(), value.v8Value());
}
template <>
WebVector<unsigned char> convertCryptoResult<WebVector<unsigned char>>(
    const ScriptValue& value) {
  WebVector<unsigned char> vector;
  if (DOMArrayBuffer* buffer = V8ArrayBuffer::toImplWithTypeCheck(
          value.isolate(), value.v8Value())) {
    vector.assign(reinterpret_cast<const unsigned char*>(buffer->data()),
                  buffer->byteLength());
  }
  return vector;
}
template <>
bool convertCryptoResult<bool>(const ScriptValue& value) {
  return value.v8Value()->IsTrue();
}

template <typename T>
class WebCryptoResultAdapter : public ScriptFunction {
 private:
  WebCryptoResultAdapter(ScriptState* scriptState,
                         std::unique_ptr<WTF::Function<void(T)>> function)
      : ScriptFunction(scriptState), m_function(std::move(function)) {}

  ScriptValue call(ScriptValue value) final {
    (*m_function)(convertCryptoResult<T>(value));
    return ScriptValue::from(getScriptState(), ToV8UndefinedGenerator());
  }

  std::unique_ptr<WTF::Function<void(T)>> m_function;
  template <typename U>
  friend WebCryptoResult toWebCryptoResult(
      ScriptState*,
      std::unique_ptr<WTF::Function<void(U)>>);
};

template <typename T>
WebCryptoResult toWebCryptoResult(
    ScriptState* scriptState,
    std::unique_ptr<WTF::Function<void(T)>> function) {
  CryptoResultImpl* result = CryptoResultImpl::create(scriptState);
  result->promise().then(
      (new WebCryptoResultAdapter<T>(scriptState, std::move(function)))
          ->bindToV8Function(),
      (new WebCryptoResultAdapter<DOMException*>(
           scriptState, WTF::bind([](DOMException* exception) {
             CHECK(false) << "crypto operation failed";
           })))
          ->bindToV8Function());
  return result->result();
}

template <typename T, typename PMF, typename... Args>
T subtleCryptoSync(ScriptState* scriptState, PMF func, Args&&... args) {
  T result;
  (Platform::current()->crypto()->*func)(
      std::forward<Args>(args)...,
      toWebCryptoResult(scriptState, WTF::bind(
                                         [](T* out, T result) {
                                           *out = result;
                                           testing::exitRunLoop();
                                         },
                                         WTF::unretained(&result))));
  testing::enterRunLoop();
  return result;
}

CryptoKey* syncGenerateKey(ScriptState* scriptState,
                           const WebCryptoAlgorithm& algorithm,
                           bool extractable,
                           WebCryptoKeyUsageMask usages) {
  return subtleCryptoSync<CryptoKey*>(scriptState, &WebCrypto::generateKey,
                                      algorithm, extractable, usages);
}

CryptoKeyPair syncGenerateKeyPair(ScriptState* scriptState,
                                  const WebCryptoAlgorithm& algorithm,
                                  bool extractable,
                                  WebCryptoKeyUsageMask usages) {
  return subtleCryptoSync<CryptoKeyPair>(scriptState, &WebCrypto::generateKey,
                                         algorithm, extractable, usages);
}

CryptoKey* syncImportKey(ScriptState* scriptState,
                         WebCryptoKeyFormat format,
                         WebVector<unsigned char> data,
                         const WebCryptoAlgorithm& algorithm,
                         bool extractable,
                         WebCryptoKeyUsageMask usages) {
  return subtleCryptoSync<CryptoKey*>(scriptState, &WebCrypto::importKey,
                                      format, data, algorithm, extractable,
                                      usages);
}

WebVector<uint8_t> syncExportKey(ScriptState* scriptState,
                                 WebCryptoKeyFormat format,
                                 const WebCryptoKey& key) {
  return subtleCryptoSync<WebVector<uint8_t>>(
      scriptState, &WebCrypto::exportKey, format, key);
}

WebVector<uint8_t> syncEncrypt(ScriptState* scriptState,
                               const WebCryptoAlgorithm& algorithm,
                               const WebCryptoKey& key,
                               WebVector<unsigned char> data) {
  return subtleCryptoSync<WebVector<uint8_t>>(scriptState, &WebCrypto::encrypt,
                                              algorithm, key, data);
}

WebVector<uint8_t> syncDecrypt(ScriptState* scriptState,
                               const WebCryptoAlgorithm& algorithm,
                               const WebCryptoKey& key,
                               WebVector<unsigned char> data) {
  return subtleCryptoSync<WebVector<uint8_t>>(scriptState, &WebCrypto::decrypt,
                                              algorithm, key, data);
}

WebVector<uint8_t> syncSign(ScriptState* scriptState,
                            const WebCryptoAlgorithm& algorithm,
                            const WebCryptoKey& key,
                            WebVector<unsigned char> message) {
  return subtleCryptoSync<WebVector<uint8_t>>(scriptState, &WebCrypto::sign,
                                              algorithm, key, message);
}

bool syncVerifySignature(ScriptState* scriptState,
                         const WebCryptoAlgorithm& algorithm,
                         const WebCryptoKey& key,
                         WebVector<unsigned char> signature,
                         WebVector<unsigned char> message) {
  return subtleCryptoSync<bool>(scriptState, &WebCrypto::verifySignature,
                                algorithm, key, signature, message);
}

WebVector<uint8_t> syncDeriveBits(ScriptState* scriptState,
                                  const WebCryptoAlgorithm& algorithm,
                                  const WebCryptoKey& key,
                                  unsigned length) {
  return subtleCryptoSync<WebVector<uint8_t>>(
      scriptState, &WebCrypto::deriveBits, algorithm, key, length);
}

// AES-128-CBC uses AES key params.
TEST(V8ScriptValueSerializerForModulesTest, RoundTripCryptoKeyAES) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  ScriptState* scriptState = scope.getScriptState();

  // Generate a 128-bit AES key.
  std::unique_ptr<WebCryptoAlgorithmParams> params(
      new WebCryptoAesKeyGenParams(128));
  WebCryptoAlgorithm algorithm(WebCryptoAlgorithmIdAesCbc, std::move(params));
  CryptoKey* key =
      syncGenerateKey(scriptState, algorithm, true,
                      WebCryptoKeyUsageEncrypt | WebCryptoKeyUsageDecrypt);

  // Round trip it and check the visible attributes.
  v8::Local<v8::Value> wrapper = ToV8(key, scope.getScriptState());
  v8::Local<v8::Value> result = roundTrip(wrapper, scope);
  ASSERT_TRUE(V8CryptoKey::hasInstance(result, scope.isolate()));
  CryptoKey* newKey = V8CryptoKey::toImpl(result.As<v8::Object>());
  EXPECT_EQ("secret", newKey->type());
  EXPECT_TRUE(newKey->extractable());
  EXPECT_THAT(newKey->usages(), UnorderedElementsAre("encrypt", "decrypt"));

  // Check that the keys have the same raw representation.
  WebVector<uint8_t> keyRaw =
      syncExportKey(scriptState, WebCryptoKeyFormatRaw, key->key());
  WebVector<uint8_t> newKeyRaw =
      syncExportKey(scriptState, WebCryptoKeyFormatRaw, newKey->key());
  EXPECT_THAT(newKeyRaw, ElementsAreArray(keyRaw));

  // Check that one can decrypt data encrypted with the other.
  Vector<unsigned char> iv(16, 0);
  WebCryptoAlgorithm encryptAlgorithm(
      WebCryptoAlgorithmIdAesCbc, WTF::makeUnique<WebCryptoAesCbcParams>(iv));
  Vector<unsigned char> plaintext{1, 2, 3};
  WebVector<uint8_t> ciphertext =
      syncEncrypt(scriptState, encryptAlgorithm, key->key(), plaintext);
  WebVector<uint8_t> newPlaintext =
      syncDecrypt(scriptState, encryptAlgorithm, newKey->key(), ciphertext);
  EXPECT_THAT(newPlaintext, ElementsAre(1, 2, 3));
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeCryptoKeyAES) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  ScriptState* scriptState = scope.getScriptState();

  // Decode a 128-bit AES key (non-extractable, decrypt only).
  RefPtr<SerializedScriptValue> input =
      serializedValue({0xff, 0x09, 0x3f, 0x00, 0x4b, 0x01, 0x01, 0x10, 0x04,
                       0x10, 0x7e, 0x25, 0xb2, 0xe8, 0x62, 0x3e, 0xd7, 0x83,
                       0x70, 0xa2, 0xae, 0x98, 0x79, 0x1b, 0xc5, 0xf7});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializerForModules(scriptState, input).deserialize();
  ASSERT_TRUE(V8CryptoKey::hasInstance(result, scope.isolate()));
  CryptoKey* newKey = V8CryptoKey::toImpl(result.As<v8::Object>());
  EXPECT_EQ("secret", newKey->type());
  EXPECT_FALSE(newKey->extractable());
  EXPECT_THAT(newKey->usages(), UnorderedElementsAre("decrypt"));

  // Check that it can successfully decrypt data.
  Vector<uint8_t> iv(16, 0);
  Vector<uint8_t> ciphertext{0x33, 0x26, 0xe7, 0x64, 0x11, 0x5e, 0xf4, 0x60,
                             0x96, 0x08, 0x11, 0xaf, 0x65, 0x8b, 0x87, 0x04};
  WebCryptoAlgorithm encryptAlgorithm(
      WebCryptoAlgorithmIdAesCbc, WTF::makeUnique<WebCryptoAesCbcParams>(iv));
  WebVector<uint8_t> plaintext =
      syncDecrypt(scriptState, encryptAlgorithm, newKey->key(), ciphertext);
  EXPECT_THAT(plaintext, ElementsAre(1, 2, 3));
}

// HMAC-SHA256 uses HMAC key params.
TEST(V8ScriptValueSerializerForModulesTest, RoundTripCryptoKeyHMAC) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  ScriptState* scriptState = scope.getScriptState();

  // Generate an HMAC-SHA256 key.
  WebCryptoAlgorithm hash(WebCryptoAlgorithmIdSha256, nullptr);
  std::unique_ptr<WebCryptoAlgorithmParams> generateKeyParams(
      new WebCryptoHmacKeyGenParams(hash, false, 0));
  WebCryptoAlgorithm generateKeyAlgorithm(WebCryptoAlgorithmIdHmac,
                                          std::move(generateKeyParams));
  CryptoKey* key =
      syncGenerateKey(scriptState, generateKeyAlgorithm, true,
                      WebCryptoKeyUsageSign | WebCryptoKeyUsageVerify);

  // Round trip it and check the visible attributes.
  v8::Local<v8::Value> wrapper = ToV8(key, scope.getScriptState());
  v8::Local<v8::Value> result = roundTrip(wrapper, scope);
  ASSERT_TRUE(V8CryptoKey::hasInstance(result, scope.isolate()));
  CryptoKey* newKey = V8CryptoKey::toImpl(result.As<v8::Object>());
  EXPECT_EQ("secret", newKey->type());
  EXPECT_TRUE(newKey->extractable());
  EXPECT_THAT(newKey->usages(), UnorderedElementsAre("sign", "verify"));

  // Check that the keys have the same raw representation.
  WebVector<uint8_t> keyRaw =
      syncExportKey(scriptState, WebCryptoKeyFormatRaw, key->key());
  WebVector<uint8_t> newKeyRaw =
      syncExportKey(scriptState, WebCryptoKeyFormatRaw, newKey->key());
  EXPECT_THAT(newKeyRaw, ElementsAreArray(keyRaw));

  // Check that one can verify a message signed by the other.
  Vector<uint8_t> message{1, 2, 3};
  WebCryptoAlgorithm algorithm(WebCryptoAlgorithmIdHmac, nullptr);
  WebVector<uint8_t> signature =
      syncSign(scriptState, algorithm, key->key(), message);
  EXPECT_TRUE(syncVerifySignature(scriptState, algorithm, newKey->key(),
                                  signature, message));
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeCryptoKeyHMAC) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  ScriptState* scriptState = scope.getScriptState();

  // Decode an HMAC-SHA256 key (non-extractable, verify only).
  RefPtr<SerializedScriptValue> input = serializedValue(
      {0xff, 0x09, 0x3f, 0x00, 0x4b, 0x02, 0x40, 0x06, 0x10, 0x40, 0xd9,
       0xbd, 0x0e, 0x84, 0x24, 0x3c, 0xb0, 0xbc, 0xee, 0x36, 0x61, 0xdc,
       0xd0, 0xb0, 0xf5, 0x62, 0x09, 0xab, 0x93, 0x8c, 0x21, 0xaf, 0xb7,
       0x66, 0xa9, 0xfc, 0xd2, 0xaa, 0xd8, 0xd4, 0x79, 0xf2, 0x55, 0x3a,
       0xef, 0x46, 0x03, 0xec, 0x64, 0x2f, 0x68, 0xea, 0x9f, 0x9d, 0x1d,
       0xd2, 0x42, 0xd0, 0x13, 0x6c, 0xe0, 0xe1, 0xed, 0x9c, 0x59, 0x46,
       0x85, 0xaf, 0x41, 0xc4, 0x6a, 0x2d, 0x06, 0x7a});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializerForModules(scriptState, input).deserialize();
  ASSERT_TRUE(V8CryptoKey::hasInstance(result, scope.isolate()));
  CryptoKey* newKey = V8CryptoKey::toImpl(result.As<v8::Object>());
  EXPECT_EQ("secret", newKey->type());
  EXPECT_FALSE(newKey->extractable());
  EXPECT_THAT(newKey->usages(), UnorderedElementsAre("verify"));

  // Check that it can successfully verify a signature.
  Vector<uint8_t> message{1, 2, 3};
  Vector<uint8_t> signature{0x91, 0xc8, 0x54, 0xc3, 0x19, 0x4e, 0xc5, 0x6c,
                            0x2d, 0x18, 0x91, 0x88, 0xd0, 0x56, 0x4d, 0xb6,
                            0x46, 0xc8, 0xb2, 0xa4, 0x2e, 0x1f, 0x0d, 0xe2,
                            0xd6, 0x60, 0xf9, 0xee, 0xb7, 0xd4, 0x55, 0x12};
  WebCryptoAlgorithm algorithm(WebCryptoAlgorithmIdHmac, nullptr);
  EXPECT_TRUE(syncVerifySignature(scriptState, algorithm, newKey->key(),
                                  signature, message));
}

// RSA-PSS-SHA256 uses RSA hashed key params.
TEST(V8ScriptValueSerializerForModulesTest, RoundTripCryptoKeyRSAHashed) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  ScriptState* scriptState = scope.getScriptState();

  // Generate an RSA-PSS-SHA256 key pair.
  WebCryptoAlgorithm hash(WebCryptoAlgorithmIdSha256, nullptr);
  std::unique_ptr<WebCryptoAlgorithmParams> generateKeyParams(
      new WebCryptoRsaHashedKeyGenParams(hash, 1024, Vector<uint8_t>{1, 0, 1}));
  WebCryptoAlgorithm generateKeyAlgorithm(WebCryptoAlgorithmIdRsaPss,
                                          std::move(generateKeyParams));
  CryptoKey* publicKey;
  CryptoKey* privateKey;
  std::tie(publicKey, privateKey) =
      syncGenerateKeyPair(scriptState, generateKeyAlgorithm, true,
                          WebCryptoKeyUsageSign | WebCryptoKeyUsageVerify);

  // Round trip the private key and check the visible attributes.
  v8::Local<v8::Value> wrapper = ToV8(privateKey, scope.getScriptState());
  v8::Local<v8::Value> result = roundTrip(wrapper, scope);
  ASSERT_TRUE(V8CryptoKey::hasInstance(result, scope.isolate()));
  CryptoKey* newPrivateKey = V8CryptoKey::toImpl(result.As<v8::Object>());
  EXPECT_EQ("private", newPrivateKey->type());
  EXPECT_TRUE(newPrivateKey->extractable());
  EXPECT_THAT(newPrivateKey->usages(), UnorderedElementsAre("sign"));

  // Check that the keys have the same PKCS8 representation.
  WebVector<uint8_t> keyRaw =
      syncExportKey(scriptState, WebCryptoKeyFormatPkcs8, privateKey->key());
  WebVector<uint8_t> newKeyRaw =
      syncExportKey(scriptState, WebCryptoKeyFormatPkcs8, newPrivateKey->key());
  EXPECT_THAT(newKeyRaw, ElementsAreArray(keyRaw));

  // Check that one can verify a message signed by the other.
  Vector<uint8_t> message{1, 2, 3};
  WebCryptoAlgorithm algorithm(WebCryptoAlgorithmIdRsaPss,
                               WTF::makeUnique<WebCryptoRsaPssParams>(16));
  WebVector<uint8_t> signature =
      syncSign(scriptState, algorithm, newPrivateKey->key(), message);
  EXPECT_TRUE(syncVerifySignature(scriptState, algorithm, publicKey->key(),
                                  signature, message));
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeCryptoKeyRSAHashed) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  ScriptState* scriptState = scope.getScriptState();

  // Decode an RSA-PSS-SHA256 public key (extractable, verify only).
  RefPtr<SerializedScriptValue> input = serializedValue(
      {0xff, 0x09, 0x3f, 0x00, 0x4b, 0x04, 0x0d, 0x01, 0x80, 0x08, 0x03, 0x01,
       0x00, 0x01, 0x06, 0x11, 0xa2, 0x01, 0x30, 0x81, 0x9f, 0x30, 0x0d, 0x06,
       0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00,
       0x03, 0x81, 0x8d, 0x00, 0x30, 0x81, 0x89, 0x02, 0x81, 0x81, 0x00, 0xae,
       0xef, 0x7f, 0xee, 0x3a, 0x48, 0x48, 0xea, 0xce, 0x18, 0x0b, 0x86, 0x34,
       0x6c, 0x1d, 0xc5, 0xe8, 0xea, 0xab, 0x33, 0xd0, 0x6f, 0x63, 0x82, 0x37,
       0x18, 0x83, 0x01, 0x3d, 0x11, 0xe3, 0x03, 0x79, 0x2c, 0x0a, 0x79, 0xe6,
       0xf5, 0x14, 0x73, 0x5f, 0x50, 0xa8, 0x17, 0x10, 0x58, 0x59, 0x20, 0x09,
       0x54, 0x56, 0xe0, 0x86, 0x07, 0x5f, 0xab, 0x9c, 0x86, 0xb1, 0x80, 0xcb,
       0x72, 0x5e, 0x55, 0x8b, 0x83, 0x98, 0xbf, 0xed, 0xbe, 0xdf, 0xdc, 0x6b,
       0xff, 0xcf, 0x50, 0xee, 0xcc, 0x7c, 0xb4, 0x8c, 0x68, 0x75, 0x66, 0xf2,
       0x21, 0x0d, 0xf5, 0x50, 0xdd, 0x06, 0x29, 0x57, 0xf7, 0x44, 0x42, 0x3d,
       0xd9, 0x30, 0xb0, 0x8a, 0x5e, 0x8f, 0xea, 0xff, 0x45, 0xa0, 0x1d, 0x04,
       0xbe, 0xc5, 0x82, 0xd3, 0x69, 0x4e, 0xcd, 0x14, 0x7b, 0xf5, 0x00, 0x3c,
       0xb1, 0x19, 0x24, 0xae, 0x8d, 0x22, 0xb5, 0x02, 0x03, 0x01, 0x00, 0x01});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializerForModules(scriptState, input).deserialize();
  ASSERT_TRUE(V8CryptoKey::hasInstance(result, scope.isolate()));
  CryptoKey* newPublicKey = V8CryptoKey::toImpl(result.As<v8::Object>());
  EXPECT_EQ("public", newPublicKey->type());
  EXPECT_TRUE(newPublicKey->extractable());
  EXPECT_THAT(newPublicKey->usages(), UnorderedElementsAre("verify"));

  // Check that it can successfully verify a signature.
  Vector<uint8_t> message{1, 2, 3};
  Vector<uint8_t> signature{
      0x9b, 0x61, 0xc8, 0x4b, 0x1c, 0xe5, 0x24, 0xe6, 0x54, 0x73, 0x1a, 0xb5,
      0xe3, 0x22, 0xc7, 0xd1, 0x36, 0x3d, 0x85, 0x99, 0x26, 0x45, 0xcc, 0x54,
      0x98, 0x1f, 0xf3, 0x9d, 0x32, 0x87, 0xdc, 0xbb, 0xb6, 0x3a, 0xa4, 0x6d,
      0xd4, 0xb5, 0x52, 0x83, 0x24, 0x02, 0xc7, 0x62, 0x1f, 0xb7, 0x27, 0x2b,
      0x5a, 0x54, 0x59, 0x17, 0x81, 0x8a, 0xf5, 0x0c, 0x17, 0x01, 0x45, 0x3f,
      0x14, 0xf2, 0x3c, 0x27, 0x4d, 0xfa, 0xc0, 0x0a, 0x82, 0x4b, 0xb2, 0xf4,
      0x7b, 0x14, 0x1b, 0xd8, 0xbc, 0xe9, 0x2e, 0xd4, 0x55, 0x27, 0x62, 0x83,
      0x11, 0xed, 0xc2, 0x81, 0x7d, 0xa9, 0x4f, 0xe0, 0xef, 0x0e, 0xa5, 0xa5,
      0xc6, 0x40, 0x46, 0xbf, 0x90, 0x19, 0xfc, 0xc8, 0x51, 0x0e, 0x0f, 0x62,
      0xeb, 0x17, 0x68, 0x1f, 0xbd, 0xfa, 0xf7, 0xd6, 0x1f, 0xa4, 0x7c, 0x9e,
      0x9e, 0xb1, 0x96, 0x8f, 0xe6, 0x5e, 0x89, 0x99};
  WebCryptoAlgorithm algorithm(WebCryptoAlgorithmIdRsaPss,
                               WTF::makeUnique<WebCryptoRsaPssParams>(16));
  EXPECT_TRUE(syncVerifySignature(scriptState, algorithm, newPublicKey->key(),
                                  signature, message));
}

// ECDSA uses EC key params.
TEST(V8ScriptValueSerializerForModulesTest, RoundTripCryptoKeyEC) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  ScriptState* scriptState = scope.getScriptState();

  // Generate an ECDSA key pair with the NIST P-256 curve.
  std::unique_ptr<WebCryptoAlgorithmParams> generateKeyParams(
      new WebCryptoEcKeyGenParams(WebCryptoNamedCurveP256));
  WebCryptoAlgorithm generateKeyAlgorithm(WebCryptoAlgorithmIdEcdsa,
                                          std::move(generateKeyParams));
  CryptoKey* publicKey;
  CryptoKey* privateKey;
  std::tie(publicKey, privateKey) =
      syncGenerateKeyPair(scriptState, generateKeyAlgorithm, true,
                          WebCryptoKeyUsageSign | WebCryptoKeyUsageVerify);

  // Round trip the private key and check the visible attributes.
  v8::Local<v8::Value> wrapper = ToV8(privateKey, scope.getScriptState());
  v8::Local<v8::Value> result = roundTrip(wrapper, scope);
  ASSERT_TRUE(V8CryptoKey::hasInstance(result, scope.isolate()));
  CryptoKey* newPrivateKey = V8CryptoKey::toImpl(result.As<v8::Object>());
  EXPECT_EQ("private", newPrivateKey->type());
  EXPECT_TRUE(newPrivateKey->extractable());
  EXPECT_THAT(newPrivateKey->usages(), UnorderedElementsAre("sign"));

  // Check that the keys have the same PKCS8 representation.
  WebVector<uint8_t> keyRaw =
      syncExportKey(scriptState, WebCryptoKeyFormatPkcs8, privateKey->key());
  WebVector<uint8_t> newKeyRaw =
      syncExportKey(scriptState, WebCryptoKeyFormatPkcs8, newPrivateKey->key());
  EXPECT_THAT(newKeyRaw, ElementsAreArray(keyRaw));

  // Check that one can verify a message signed by the other.
  WebCryptoAlgorithm hash(WebCryptoAlgorithmIdSha256, nullptr);
  Vector<uint8_t> message{1, 2, 3};
  WebCryptoAlgorithm algorithm(WebCryptoAlgorithmIdEcdsa,
                               WTF::makeUnique<WebCryptoEcdsaParams>(hash));
  WebVector<uint8_t> signature =
      syncSign(scriptState, algorithm, newPrivateKey->key(), message);
  EXPECT_TRUE(syncVerifySignature(scriptState, algorithm, publicKey->key(),
                                  signature, message));
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeCryptoKeyEC) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  ScriptState* scriptState = scope.getScriptState();

  // Decode an ECDSA public key with the NIST P-256 curve (extractable).
  RefPtr<SerializedScriptValue> input = serializedValue(
      {0xff, 0x09, 0x3f, 0x00, 0x4b, 0x05, 0x0e, 0x01, 0x01, 0x11, 0x5b, 0x30,
       0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01,
       0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03, 0x42,
       0x00, 0x04, 0xfe, 0x16, 0x70, 0x29, 0x07, 0x2c, 0x11, 0xbf, 0xcf, 0xb7,
       0x9d, 0x54, 0x35, 0x3d, 0xc7, 0x85, 0x66, 0x26, 0xa5, 0xda, 0x69, 0x4c,
       0x07, 0xd5, 0x74, 0xcb, 0x93, 0xf4, 0xdb, 0x7e, 0x38, 0x3c, 0xa8, 0x98,
       0x2a, 0x6f, 0xb2, 0xf5, 0x48, 0x73, 0x2f, 0x59, 0x21, 0xa0, 0xa9, 0xf5,
       0x6e, 0x37, 0x0c, 0xfc, 0x5b, 0x68, 0x0e, 0x19, 0x5b, 0xd3, 0x4f, 0xb4,
       0x0e, 0x1c, 0x31, 0x5a, 0xaa, 0x2d});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializerForModules(scriptState, input).deserialize();
  ASSERT_TRUE(V8CryptoKey::hasInstance(result, scope.isolate()));
  CryptoKey* newPublicKey = V8CryptoKey::toImpl(result.As<v8::Object>());
  EXPECT_EQ("public", newPublicKey->type());
  EXPECT_TRUE(newPublicKey->extractable());
  EXPECT_THAT(newPublicKey->usages(), UnorderedElementsAre("verify"));

  // Check that it can successfully verify a signature.
  Vector<uint8_t> message{1, 2, 3};
  Vector<uint8_t> signature{
      0xee, 0x63, 0xa2, 0xa3, 0x87, 0x6c, 0x9f, 0xc5, 0x64, 0x12, 0x87,
      0x0d, 0xc7, 0xff, 0x3c, 0xd2, 0x6c, 0x2b, 0x2c, 0x0b, 0x2b, 0x8d,
      0x3c, 0xe0, 0x3f, 0xd3, 0xfc, 0x28, 0xf0, 0xa1, 0x22, 0x69, 0x0a,
      0x33, 0x4d, 0x48, 0x97, 0xad, 0x67, 0xa9, 0x6e, 0x24, 0xe7, 0x31,
      0x09, 0xdb, 0xa8, 0x92, 0x48, 0x70, 0xa6, 0x6c, 0x46, 0x4d, 0x0b,
      0x83, 0x27, 0x37, 0x69, 0x4d, 0x32, 0x63, 0x1e, 0x82};
  WebCryptoAlgorithm hash(WebCryptoAlgorithmIdSha256, nullptr);
  WebCryptoAlgorithm algorithm(WebCryptoAlgorithmIdEcdsa,
                               WTF::makeUnique<WebCryptoEcdsaParams>(hash));
  EXPECT_TRUE(syncVerifySignature(scriptState, algorithm, newPublicKey->key(),
                                  signature, message));
}

TEST(V8ScriptValueSerializerForModulesTest, RoundTripCryptoKeyNoParams) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  ScriptState* scriptState = scope.getScriptState();

  // Import some data into a PBKDF2 state.
  WebCryptoAlgorithm importKeyAlgorithm(WebCryptoAlgorithmIdPbkdf2, nullptr);
  CryptoKey* key = syncImportKey(scriptState, WebCryptoKeyFormatRaw,
                                 Vector<uint8_t>{1, 2, 3}, importKeyAlgorithm,
                                 false, WebCryptoKeyUsageDeriveBits);

  // Round trip the key and check the visible attributes.
  v8::Local<v8::Value> wrapper = ToV8(key, scope.getScriptState());
  v8::Local<v8::Value> result = roundTrip(wrapper, scope);
  ASSERT_TRUE(V8CryptoKey::hasInstance(result, scope.isolate()));
  CryptoKey* newKey = V8CryptoKey::toImpl(result.As<v8::Object>());
  EXPECT_EQ("secret", newKey->type());
  EXPECT_FALSE(newKey->extractable());
  EXPECT_THAT(newKey->usages(), UnorderedElementsAre("deriveBits"));

  // Check that the keys derive the same bits.
  WebCryptoAlgorithm hash(WebCryptoAlgorithmIdSha256, nullptr);
  WebVector<uint8_t> salt(static_cast<size_t>(16));
  std::unique_ptr<WebCryptoAlgorithmParams> params(
      new WebCryptoPbkdf2Params(hash, salt, 1));
  WebCryptoAlgorithm algorithm(WebCryptoAlgorithmIdPbkdf2, std::move(params));
  WebVector<uint8_t> bitsRaw =
      syncDeriveBits(scriptState, algorithm, key->key(), 16);
  WebVector<uint8_t> newBitsRaw =
      syncDeriveBits(scriptState, algorithm, newKey->key(), 16);
  EXPECT_EQ(2u, bitsRaw.size());
  EXPECT_THAT(newBitsRaw, ElementsAreArray(bitsRaw));
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeCryptoKeyNoParams) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  ScriptState* scriptState = scope.getScriptState();

  // Decode PBKDF2 state seeded with {1,2,3}.
  RefPtr<SerializedScriptValue> input =
      serializedValue({0xff, 0x09, 0x3f, 0x00, 0x4b, 0x06, 0x11, 0xa0, 0x02,
                       0x03, 0x01, 0x02, 0x03, 0x00});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializerForModules(scriptState, input).deserialize();
  ASSERT_TRUE(V8CryptoKey::hasInstance(result, scope.isolate()));
  CryptoKey* newKey = V8CryptoKey::toImpl(result.As<v8::Object>());
  EXPECT_EQ("secret", newKey->type());
  EXPECT_FALSE(newKey->extractable());
  EXPECT_THAT(newKey->usages(),
              UnorderedElementsAre("deriveKey", "deriveBits"));

  // Check that it derives the right bits.
  WebCryptoAlgorithm hash(WebCryptoAlgorithmIdSha256, nullptr);
  WebVector<uint8_t> salt(static_cast<size_t>(16));
  std::unique_ptr<WebCryptoAlgorithmParams> params(
      new WebCryptoPbkdf2Params(hash, salt, 3));
  WebCryptoAlgorithm algorithm(WebCryptoAlgorithmIdPbkdf2, std::move(params));
  WebVector<uint8_t> bitsRaw =
      syncDeriveBits(scriptState, algorithm, newKey->key(), 32);
  EXPECT_THAT(bitsRaw, ElementsAre(0xd8, 0x0e, 0x2f, 0x69));
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeCryptoKeyInvalid) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  ScriptState* scriptState = scope.getScriptState();

  // Invalid algorithm ID.
  EXPECT_TRUE(V8ScriptValueDeserializerForModules(
                  scriptState,
                  serializedValue({0xff, 0x09, 0x3f, 0x00, 0x4b, 0x06, 0x7f,
                                   0xa0, 0x02, 0x03, 0x01, 0x02, 0x03, 0x00}))
                  .deserialize()
                  ->IsNull());

  // Algorithm ID / params type mismatch (AES params, RSA-OEAP ID).
  EXPECT_TRUE(
      V8ScriptValueDeserializerForModules(
          scriptState,
          serializedValue({0xff, 0x09, 0x3f, 0x00, 0x4b, 0x01, 0x0a, 0x10, 0x04,
                           0x10, 0x7e, 0x25, 0xb2, 0xe8, 0x62, 0x3e, 0xd7, 0x83,
                           0x70, 0xa2, 0xae, 0x98, 0x79, 0x1b, 0xc5, 0xf7}))
          .deserialize()
          ->IsNull());

  // Invalid asymmetric key type.
  EXPECT_TRUE(
      V8ScriptValueDeserializerForModules(
          scriptState,
          serializedValue(
              {0xff, 0x09, 0x3f, 0x00, 0x4b, 0x05, 0x0e, 0x7f, 0x01, 0x11, 0x5b,
               0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d,
               0x02, 0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01,
               0x07, 0x03, 0x42, 0x00, 0x04, 0xfe, 0x16, 0x70, 0x29, 0x07, 0x2c,
               0x11, 0xbf, 0xcf, 0xb7, 0x9d, 0x54, 0x35, 0x3d, 0xc7, 0x85, 0x66,
               0x26, 0xa5, 0xda, 0x69, 0x4c, 0x07, 0xd5, 0x74, 0xcb, 0x93, 0xf4,
               0xdb, 0x7e, 0x38, 0x3c, 0xa8, 0x98, 0x2a, 0x6f, 0xb2, 0xf5, 0x48,
               0x73, 0x2f, 0x59, 0x21, 0xa0, 0xa9, 0xf5, 0x6e, 0x37, 0x0c, 0xfc,
               0x5b, 0x68, 0x0e, 0x19, 0x5b, 0xd3, 0x4f, 0xb4, 0x0e, 0x1c, 0x31,
               0x5a, 0xaa, 0x2d}))
          .deserialize()
          ->IsNull());

  // Invalid named curve.
  EXPECT_TRUE(
      V8ScriptValueDeserializerForModules(
          scriptState,
          serializedValue(
              {0xff, 0x09, 0x3f, 0x00, 0x4b, 0x05, 0x0e, 0x01, 0x7f, 0x11, 0x5b,
               0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d,
               0x02, 0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01,
               0x07, 0x03, 0x42, 0x00, 0x04, 0xfe, 0x16, 0x70, 0x29, 0x07, 0x2c,
               0x11, 0xbf, 0xcf, 0xb7, 0x9d, 0x54, 0x35, 0x3d, 0xc7, 0x85, 0x66,
               0x26, 0xa5, 0xda, 0x69, 0x4c, 0x07, 0xd5, 0x74, 0xcb, 0x93, 0xf4,
               0xdb, 0x7e, 0x38, 0x3c, 0xa8, 0x98, 0x2a, 0x6f, 0xb2, 0xf5, 0x48,
               0x73, 0x2f, 0x59, 0x21, 0xa0, 0xa9, 0xf5, 0x6e, 0x37, 0x0c, 0xfc,
               0x5b, 0x68, 0x0e, 0x19, 0x5b, 0xd3, 0x4f, 0xb4, 0x0e, 0x1c, 0x31,
               0x5a, 0xaa, 0x2d}))
          .deserialize()
          ->IsNull());

  // Unknown usage.
  EXPECT_TRUE(V8ScriptValueDeserializerForModules(
                  scriptState,
                  serializedValue({0xff, 0x09, 0x3f, 0x00, 0x4b, 0x06, 0x11,
                                   0x80, 0x40, 0x03, 0x01, 0x02, 0x03, 0x00}))
                  .deserialize()
                  ->IsNull());

  // AES key length (16384) that would overflow unsigned short after multiply by
  // 8 (to convert from bytes to bits).
  EXPECT_TRUE(V8ScriptValueDeserializerForModules(
                  scriptState,
                  serializedValue({0xff, 0x09, 0x3f, 0x00, 0x4b, 0x01, 0x01,
                                   0x80, 0x80, 0x02, 0x04, 0x10, 0x7e, 0x25,
                                   0xb2, 0xe8, 0x62, 0x3e, 0xd7, 0x83, 0x70,
                                   0xa2, 0xae, 0x98, 0x79, 0x1b, 0xc5, 0xf7}))
                  .deserialize()
                  ->IsNull());

  // HMAC length (1073741824) that would overflow 32-bit unsigned after multiply
  // by 8 (to convert from bytes to bits).
  EXPECT_TRUE(
      V8ScriptValueDeserializerForModules(
          scriptState,
          serializedValue({0xff, 0x09, 0x3f, 0x00, 0x4b, 0x02, 0x80, 0x80, 0x80,
                           0x80, 0x04, 0x06, 0x10, 0x40, 0xd9, 0xbd, 0x0e, 0x84,
                           0x24, 0x3c, 0xb0, 0xbc, 0xee, 0x36, 0x61, 0xdc, 0xd0,
                           0xb0, 0xf5, 0x62, 0x09, 0xab, 0x93, 0x8c, 0x21, 0xaf,
                           0xb7, 0x66, 0xa9, 0xfc, 0xd2, 0xaa, 0xd8, 0xd4, 0x79,
                           0xf2, 0x55, 0x3a, 0xef, 0x46, 0x03, 0xec, 0x64, 0x2f,
                           0x68, 0xea, 0x9f, 0x9d, 0x1d, 0xd2, 0x42, 0xd0, 0x13,
                           0x6c, 0xe0, 0xe1, 0xed, 0x9c, 0x59, 0x46, 0x85, 0xaf,
                           0x41, 0xc4, 0x6a, 0x2d, 0x06, 0x7a}))
          .deserialize()
          ->IsNull());

  // Input ends before end of declared public exponent size.
  EXPECT_TRUE(
      V8ScriptValueDeserializerForModules(
          scriptState, serializedValue({0xff, 0x09, 0x3f, 0x00, 0x4b, 0x04,
                                        0x0d, 0x01, 0x80, 0x08, 0x03, 0x01}))
          .deserialize()
          ->IsNull());
}

TEST(V8ScriptValueSerializerForModulesTest, RoundTripDOMFileSystem) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;

  DOMFileSystem* fs = DOMFileSystem::create(
      scope.getExecutionContext(), "http_example.com_0:Persistent",
      FileSystemTypePersistent,
      KURL(ParsedURLString, "filesystem:http://example.com/persistent/"));
  // At time of writing, this can only happen for filesystems from PPAPI.
  fs->makeClonable();
  v8::Local<v8::Value> wrapper = ToV8(fs, scope.getScriptState());
  v8::Local<v8::Value> result = roundTrip(wrapper, scope);
  ASSERT_FALSE(result.IsEmpty());
  ASSERT_TRUE(V8DOMFileSystem::hasInstance(result, scope.isolate()));
  DOMFileSystem* newFS = V8DOMFileSystem::toImpl(result.As<v8::Object>());
  EXPECT_EQ("http_example.com_0:Persistent", newFS->name());
  EXPECT_EQ(FileSystemTypePersistent, newFS->type());
  EXPECT_EQ("filesystem:http://example.com/persistent/",
            newFS->rootURL().getString());
}

TEST(V8ScriptValueSerializerForModulesTest, RoundTripDOMFileSystemNotClonable) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  ExceptionState exceptionState(scope.isolate(),
                                ExceptionState::ExecutionContext, "Window",
                                "postMessage");

  DOMFileSystem* fs = DOMFileSystem::create(
      scope.getExecutionContext(), "http_example.com_0:Persistent",
      FileSystemTypePersistent,
      KURL(ParsedURLString, "filesystem:http://example.com/persistent/0/"));
  ASSERT_FALSE(fs->clonable());
  v8::Local<v8::Value> wrapper = ToV8(fs, scope.getScriptState());
  EXPECT_FALSE(V8ScriptValueSerializer(scope.getScriptState())
                   .serialize(wrapper, nullptr, exceptionState));
  EXPECT_TRUE(hadDOMException("DataCloneError", scope.getScriptState(),
                              exceptionState));
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeDOMFileSystem) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;

  // This is encoded data generated from Chromium (around M56).
  ScriptState* scriptState = scope.getScriptState();
  RefPtr<SerializedScriptValue> input = serializedValue(
      {0xff, 0x09, 0x3f, 0x00, 0x64, 0x01, 0x1d, 0x68, 0x74, 0x74, 0x70, 0x5f,
       0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d, 0x5f,
       0x30, 0x3a, 0x50, 0x65, 0x72, 0x73, 0x69, 0x73, 0x74, 0x65, 0x6e, 0x74,
       0x29, 0x66, 0x69, 0x6c, 0x65, 0x73, 0x79, 0x73, 0x74, 0x65, 0x6d, 0x3a,
       0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x65, 0x78, 0x61, 0x6d, 0x70,
       0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d, 0x2f, 0x70, 0x65, 0x72, 0x73, 0x69,
       0x73, 0x74, 0x65, 0x6e, 0x74, 0x2f});

  // Decode test.
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializerForModules(scriptState, input).deserialize();
  ASSERT_TRUE(V8DOMFileSystem::hasInstance(result, scope.isolate()));
  DOMFileSystem* newFS = V8DOMFileSystem::toImpl(result.As<v8::Object>());
  EXPECT_EQ("http_example.com_0:Persistent", newFS->name());
  EXPECT_EQ(FileSystemTypePersistent, newFS->type());
  EXPECT_EQ("filesystem:http://example.com/persistent/",
            newFS->rootURL().getString());
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeInvalidDOMFileSystem) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  ScriptState* scriptState = scope.getScriptState();

  // Filesystem type out of range.
  EXPECT_TRUE(
      V8ScriptValueDeserializerForModules(
          scriptState,
          serializedValue({0xff, 0x09, 0x3f, 0x00, 0x64, 0x04, 0x1d, 0x68, 0x74,
                           0x74, 0x70, 0x5f, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c,
                           0x65, 0x2e, 0x63, 0x6f, 0x6d, 0x5f, 0x30, 0x3a, 0x50,
                           0x65, 0x72, 0x73, 0x69, 0x73, 0x74, 0x65, 0x6e, 0x74,
                           0x29, 0x66, 0x69, 0x6c, 0x65, 0x73, 0x79, 0x73, 0x74,
                           0x65, 0x6d, 0x3a, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f,
                           0x2f, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e,
                           0x63, 0x6f, 0x6d, 0x2f, 0x70, 0x65, 0x72, 0x73, 0x69,
                           0x73, 0x74, 0x65, 0x6e, 0x74, 0x2f

          }))
          .deserialize()
          ->IsNull());
}

}  // namespace
}  // namespace blink
