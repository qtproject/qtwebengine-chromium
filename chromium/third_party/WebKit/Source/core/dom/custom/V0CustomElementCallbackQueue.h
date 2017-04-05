/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef V0CustomElementCallbackQueue_h
#define V0CustomElementCallbackQueue_h

#include "core/dom/Element.h"
#include "core/dom/custom/V0CustomElementProcessingStep.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"

namespace blink {

// FIXME: Rename this because it contains resolution and upgrade as
// well as callbacks.
class V0CustomElementCallbackQueue
    : public GarbageCollected<V0CustomElementCallbackQueue> {
  WTF_MAKE_NONCOPYABLE(V0CustomElementCallbackQueue);

 public:
  static V0CustomElementCallbackQueue* create(Element*);

  typedef int ElementQueueId;
  ElementQueueId owner() const { return m_owner; }

  void setOwner(ElementQueueId newOwner) {
    // ElementCallbackQueues only migrate towards the top of the
    // processing stack.
    DCHECK_GE(newOwner, m_owner);
    m_owner = newOwner;
  }

  bool processInElementQueue(ElementQueueId);

  void append(V0CustomElementProcessingStep* invocation) {
    m_queue.push_back(invocation);
  }
  bool inCreatedCallback() const { return m_inCreatedCallback; }

  DECLARE_TRACE();

 private:
  explicit V0CustomElementCallbackQueue(Element*);

  Member<Element> m_element;
  HeapVector<Member<V0CustomElementProcessingStep>> m_queue;
  ElementQueueId m_owner;
  size_t m_index;
  bool m_inCreatedCallback;
};

}  // namespace blink

#endif  // V0CustomElementCallbackQueue_h
