// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/cfwl_evttextfull.h"

CFWL_EvtTextFull::CFWL_EvtTextFull() {}

CFWL_EvtTextFull::~CFWL_EvtTextFull() {}

CFWL_EventType CFWL_EvtTextFull::GetClassID() const {
  return CFWL_EventType::TextFull;
}
