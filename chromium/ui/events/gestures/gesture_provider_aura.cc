// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gestures/gesture_provider_aura.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/gesture_detection/gesture_event_data.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"

namespace ui {

GestureProviderAura::GestureProviderAura(GestureConsumer* consumer,
                                         GestureProviderAuraClient* client)
    : client_(client),
      filtered_gesture_provider_(
          GetGestureProviderConfig(GestureProviderConfigType::CURRENT_PLATFORM),
          this),
      handling_event_(false),
      gesture_consumer_(consumer) {
  filtered_gesture_provider_.SetDoubleTapSupportForPlatformEnabled(false);
}

GestureProviderAura::~GestureProviderAura() {}

bool GestureProviderAura::OnTouchEvent(TouchEvent* event) {
  if (!pointer_state_.OnTouch(*event))
    return false;

  auto result = filtered_gesture_provider_.OnTouchEvent(pointer_state_);
  if (!result.succeeded)
    return false;

  event->set_may_cause_scrolling(result.moved_beyond_slop_region);
  pointer_state_.CleanupRemovedTouchPoints(*event);
  return true;
}

void GestureProviderAura::OnTouchEventAck(uint32_t unique_touch_event_id,
    bool event_consumed) {
  DCHECK(pending_gestures_.empty());
  DCHECK(!handling_event_);
  base::AutoReset<bool> handling_event(&handling_event_, true);
  filtered_gesture_provider_.OnTouchEventAck(unique_touch_event_id,
      event_consumed);
}

void GestureProviderAura::OnGestureEvent(const GestureEventData& gesture) {
  std::unique_ptr<ui::GestureEvent> event(
      new ui::GestureEvent(gesture.x, gesture.y, gesture.flags,
                           gesture.time, gesture.details,
                           gesture.unique_touch_event_id));

  if (!handling_event_) {
    // Dispatching event caused by timer.
    client_->OnGestureEvent(gesture_consumer_, event.get());
  } else {
    pending_gestures_.push_back(std::move(event));
  }
}

std::vector<std::unique_ptr<GestureEvent>>
GestureProviderAura::GetAndResetPendingGestures() {
  std::vector<std::unique_ptr<GestureEvent>> result;
  result.swap(pending_gestures_);
  return result;
}

void GestureProviderAura::OnTouchEnter(int pointer_id, float x, float y) {
  std::unique_ptr<TouchEvent> touch_event(new TouchEvent(
      ET_TOUCH_PRESSED, gfx::Point(), EF_IS_SYNTHESIZED, pointer_id,
      ui::EventTimeForNow(), 0.0f, 0.0f, 0.0f, 0.0f));
  gfx::PointF point(x, y);
  touch_event->set_location_f(point);
  touch_event->set_root_location_f(point);

  OnTouchEvent(touch_event.get());
  OnTouchEventAck(touch_event->unique_event_id(), true);
}

}  // namespace content
