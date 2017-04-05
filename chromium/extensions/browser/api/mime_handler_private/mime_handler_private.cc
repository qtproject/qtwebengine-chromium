// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/mime_handler_private/mime_handler_private.h"

#include <unordered_map>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/browser/stream_info.h"
#include "content/public/common/content_constants.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/common/constants.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/http/http_response_headers.h"

namespace extensions {
namespace {

std::unordered_map<std::string, std::string> CreateResponseHeadersMap(
    const net::HttpResponseHeaders* headers) {
  std::unordered_map<std::string, std::string> result;
  if (!headers)
    return result;

  size_t iter = 0;
  std::string header_name;
  std::string header_value;
  while (headers->EnumerateHeaderLines(&iter, &header_name, &header_value)) {
    // mojo strings must be UTF-8 and headers might not be, so drop any headers
    // that aren't ASCII. The PDF plugin does not use any headers with non-ASCII
    // names and non-ASCII values are never useful for the headers the plugin
    // does use.
    //
    // TODO(sammc): Send as bytes instead of a string and let the client decide
    // how to decode.
    if (!base::IsStringASCII(header_name) || !base::IsStringASCII(header_value))
      continue;
    auto& current_value = result[header_name];
    if (!current_value.empty())
      current_value += ", ";
    current_value += header_value;
  }
  return result;
}

}  // namespace

MimeHandlerServiceImpl::MimeHandlerServiceImpl(
    base::WeakPtr<StreamContainer> stream_container)
    : stream_(stream_container), weak_factory_(this) {}

MimeHandlerServiceImpl::~MimeHandlerServiceImpl() {}

// static
void MimeHandlerServiceImpl::Create(
    base::WeakPtr<StreamContainer> stream_container,
    mime_handler::MimeHandlerServiceRequest request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<MimeHandlerServiceImpl>(stream_container),
      std::move(request));
}

void MimeHandlerServiceImpl::GetStreamInfo(
    const GetStreamInfoCallback& callback) {
  if (!stream_) {
    callback.Run(mime_handler::StreamInfoPtr());
    return;
  }
  callback.Run(mojo::ConvertTo<mime_handler::StreamInfoPtr>(*stream_));
}

void MimeHandlerServiceImpl::AbortStream(const AbortStreamCallback& callback) {
  if (!stream_) {
    callback.Run();
    return;
  }
  stream_->Abort(base::Bind(&MimeHandlerServiceImpl::OnStreamClosed,
                            weak_factory_.GetWeakPtr(), callback));
}

void MimeHandlerServiceImpl::OnStreamClosed(
    const AbortStreamCallback& callback) {
  callback.Run();
}

}  // namespace extensions

namespace mojo {

extensions::mime_handler::StreamInfoPtr TypeConverter<
    extensions::mime_handler::StreamInfoPtr,
    extensions::StreamContainer>::Convert(const extensions::StreamContainer&
                                              stream) {
  if (!stream.stream_info()->handle)
    return extensions::mime_handler::StreamInfoPtr();

  auto result = extensions::mime_handler::StreamInfo::New();
  result->embedded = stream.embedded();
  result->tab_id = stream.tab_id();
  const content::StreamInfo* info = stream.stream_info();
  result->mime_type = info->mime_type;

  // If the URL is too long, mojo will give up on sending the URL. In these
  // cases truncate it. Only data: URLs should ever really suffer this problem
  // so only worry about those for now.
  // TODO(raymes): This appears to be a bug in mojo somewhere. crbug.com/480099.
  if (info->original_url.SchemeIs(url::kDataScheme) &&
      info->original_url.spec().size() > content::kMaxURLDisplayChars) {
    result->original_url = info->original_url.scheme() + ":";
  } else {
    result->original_url = info->original_url.spec();
  }

  result->stream_url = info->handle->GetURL().spec();
  result->response_headers =
      extensions::CreateResponseHeadersMap(info->response_headers.get());
  return result;
}

}  // namespace mojo
