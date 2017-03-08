// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/status.h"

namespace ntp_snippets {

Status::Status(StatusCode status, const std::string& message)
    : status(status), message(message) {}

Status::Status(StatusCode status) : Status(status, std::string()) {}

Status::~Status() = default;

}  // namespace ntp_snippets
