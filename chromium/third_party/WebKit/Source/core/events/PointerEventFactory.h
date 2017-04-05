// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PointerEventFactory_h
#define PointerEventFactory_h

#include "core/CoreExport.h"
#include "core/events/PointerEvent.h"
#include "public/platform/WebPointerProperties.h"
#include "wtf/Allocator.h"
#include "wtf/HashMap.h"

namespace blink {

// Helper class for tracking the pointer ids for each type of PointerEvents.
// Using id re-mapping at this layer, this class makes sure that regardless of
// the domain of the id (i.e. in touch or pen) the corresponding pointer event
// will have a unique id amongst all pointer events as per pointer events'
// specification. This class intended to behave the same as existing browsers as
// much as possible for compatibility reasons. Particularly it behaves very
// similar to MS Edge to have pointer event ids generated by mouse always equal
// 1 and those that are generated by touch and pen will have increasing ids from
// 2.
class CORE_EXPORT PointerEventFactory {
  DISALLOW_NEW();

 public:
  PointerEventFactory();
  ~PointerEventFactory();

  PointerEvent* create(const AtomicString& mouseEventName,
                       const PlatformMouseEvent&,
                       const Vector<PlatformMouseEvent>&,
                       LocalDOMWindow*);

  PointerEvent* create(const PlatformTouchPoint&,
                       const Vector<PlatformTouchPoint>&,
                       PlatformEvent::Modifiers,
                       LocalFrame*,
                       DOMWindow*);

  PointerEvent* createPointerCancelEvent(
      const int pointerId,
      const WebPointerProperties::PointerType);

  // For creating capture events (i.e got/lostpointercapture)
  PointerEvent* createPointerCaptureEvent(PointerEvent*, const AtomicString&);

  // For creating boundary events (i.e pointerout/leave/over/enter)
  PointerEvent* createPointerBoundaryEvent(PointerEvent*,
                                           const AtomicString&,
                                           EventTarget*);

  // Clear all the existing ids.
  void clear();

  // When a particular pointerId is removed, the id is considered free even
  // though there might have been other PointerEvents that were generated with
  // the same id before.
  bool remove(const int);

  // Returns all ids of the given pointerType.
  Vector<int> getPointerIdsOfType(WebPointerProperties::PointerType) const;

  // Returns whether a pointer id exists and active.
  bool isActive(const int) const;

  // Returns whether a pointer id exists and has at least one pressed button.
  bool isActiveButtonsState(const int) const;

  // Returns the id of the pointer event corresponding to the given pointer
  // properties if exists otherwise s_invalidId.
  int getPointerEventId(const WebPointerProperties&) const;

  // Returns pointerType of for the given pointerId if such id is active.
  // Otherwise it returns WebPointerProperties::PointerType::Unknown.
  WebPointerProperties::PointerType getPointerType(int pointerId) const;

  static const int s_mouseId;

 private:
  typedef WTF::UnsignedWithZeroKeyHashTraits<int> UnsignedHash;
  typedef struct IncomingId : public std::pair<int, int> {
    IncomingId() {}
    IncomingId(WebPointerProperties::PointerType pointerType, int rawId)
        : std::pair<int, int>(static_cast<int>(pointerType), rawId) {}
    int pointerTypeInt() const { return first; }
    WebPointerProperties::PointerType pointerType() const {
      return static_cast<WebPointerProperties::PointerType>(first);
    }
    int rawId() const { return second; }
  } IncomingId;
  typedef struct PointerAttributes {
    IncomingId incomingId;
    bool isActiveButtons;
    PointerAttributes() : incomingId(), isActiveButtons(false) {}
    PointerAttributes(IncomingId incomingId, unsigned isActiveButtons)
        : incomingId(incomingId), isActiveButtons(isActiveButtons) {}
  } PointerAttributes;

  int addIdAndActiveButtons(const IncomingId, bool isActiveButtons);
  bool isPrimary(const int) const;
  void setIdTypeButtons(PointerEventInit&,
                        const WebPointerProperties&,
                        unsigned buttons);
  void setEventSpecificFields(PointerEventInit&, const AtomicString& type);

  // Creates pointerevents like boundary and capture events from another
  // pointerevent (i.e. up/down/move events).
  PointerEvent* createPointerEventFrom(PointerEvent*,
                                       const AtomicString&,
                                       EventTarget*);

  static const int s_invalidId;

  int m_currentId;
  HashMap<IncomingId,
          int,
          WTF::PairHash<int, int>,
          WTF::PairHashTraits<UnsignedHash, UnsignedHash>>
      m_pointerIncomingIdMapping;
  HashMap<int, PointerAttributes, WTF::IntHash<int>, UnsignedHash>
      m_pointerIdMapping;
  int m_primaryId[static_cast<int>(
                      WebPointerProperties::PointerType::LastEntry) +
                  1];
  int m_idCount[static_cast<int>(WebPointerProperties::PointerType::LastEntry) +
                1];
};

}  // namespace blink

#endif  // PointerEventFactory_h
