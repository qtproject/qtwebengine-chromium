// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_RESOURCE_ID_H_
#define CC_BASE_RESOURCE_ID_H_

#include <stdint.h>
#include <unordered_set>

namespace cc {

using ResourceId = uint32_t;
using ResourceIdSet = std::unordered_set<ResourceId>;

}  // namespace cc

#endif  // CC_BASE_RESOURCE_ID_H_
