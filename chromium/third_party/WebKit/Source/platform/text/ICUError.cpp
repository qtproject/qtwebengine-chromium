// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/text/ICUError.h"

namespace blink {

// Distinguish memory allocation failures from other errors.
// https://groups.google.com/a/chromium.org/d/msg/platform-architecture-dev/MP0k9WGnCjA/zIBiJtilBwAJ
static NEVER_INLINE void ICUOutOfMemory() {
  OOM_CRASH();
}

void ICUError::handleFailure() {
  switch (m_error) {
    case U_MEMORY_ALLOCATION_ERROR:
      ICUOutOfMemory();
      break;
    case U_ILLEGAL_ARGUMENT_ERROR:
      CHECK(false) << m_error;
      break;
    default:
      break;
  }
}

}  // namespace blink
