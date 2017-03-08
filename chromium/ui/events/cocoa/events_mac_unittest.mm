// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "ui/events/cocoa/cocoa_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#import "ui/events/test/cocoa_test_event_utils.h"
#include "ui/gfx/geometry/point.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"

namespace ui {

namespace {

// Although CGEventFlags is just a typedef to int in 10.10 and earlier headers,
// the 10.11 header makes this a CF_ENUM, but doesn't give an option for "none".
const CGEventFlags kNoEventFlags = static_cast<CGEventFlags>(0);

class EventsMacTest : public CocoaTest {
 public:
  EventsMacTest() {}

  gfx::Point Flip(gfx::Point window_location) {
    NSRect window_frame = [test_window() frame];
    CGFloat content_height =
        NSHeight([test_window() contentRectForFrameRect:window_frame]);
    window_location.set_y(content_height - window_location.y());
    return window_location;
  }

  // TODO(tapted): Move this to cocoa_test_event_utils. It's not a drop-in
  // replacement because -[NSApp sendEvent:] may route events generated this way
  // differently.
  NSEvent* TestMouseEvent(CGEventType type,
                          const gfx::Point& window_location,
                          CGEventFlags event_flags) {
    // CGEventCreateMouseEvent() ignores the CGMouseButton parameter unless
    // |type| is one of kCGEventOtherMouse{Up,Down,Dragged}. It can be an
    // integer up to 31. However, constants are only supplied up to 2. For now,
    // just assume "other" means the third/center mouse button, and rely on
    // Quartz ignoring it when the type is not "other".
    CGMouseButton other_button = kCGMouseButtonCenter;
    CGPoint screen_point = cocoa_test_event_utils::ScreenPointFromWindow(
        Flip(window_location).ToCGPoint(), test_window());
    base::ScopedCFTypeRef<CGEventRef> mouse(
        CGEventCreateMouseEvent(nullptr, type, screen_point, other_button));
    CGEventSetFlags(mouse, event_flags);
    return cocoa_test_event_utils::AttachWindowToCGEvent(mouse, test_window());
  }

  // Creates a scroll event from a "real" mouse wheel (i.e. not a trackpad).
  NSEvent* TestScrollEvent(const gfx::Point& window_location,
                           int32_t delta_x,
                           int32_t delta_y) {
    bool precise = false;
    return cocoa_test_event_utils::TestScrollEvent(
        Flip(window_location).ToCGPoint(), test_window(), delta_x, delta_y,
        precise, NSEventPhaseNone, NSEventPhaseNone);
  }

  // Creates the sequence of events generated by a trackpad scroll.
  // |initial_rest| indicates whether there is a pause before scrolling starts.
  // |delta_y| is the portion to scroll without momentum (fingers on the
  // trackpad). |momentum_delta_y| is the momentum portion. A zero delta skips
  // that phase (if both are zero, the |initial_rest| is cancelled).
  NSArray* TrackpadScrollSequence(bool initial_rest,
                                  int32_t delta_y,
                                  int32_t momentum_delta_y);

 protected:
  const gfx::Point default_location_ = gfx::Point(10, 20);

 private:
  DISALLOW_COPY_AND_ASSIGN(EventsMacTest);
};

// Trackpad scroll sequences below determined empirically on OSX 10.11 (linking
// to 10.10 SDK), and dumping out with NSLog in -[NSView scrollWheel:]. First
// created using a Magic Trackpad 2 on a MacPro. See the Trackpad* test cases
// below for example event streams.
NSArray* EventsMacTest::TrackpadScrollSequence(bool initial_rest,
                                               int32_t delta_y,
                                               int32_t momentum_delta_y) {
  int32_t delta_x = 0;  // Just test vertical scrolling for now.
  base::scoped_nsobject<NSMutableArray> events([[NSMutableArray alloc] init]);

  // Resting part.
  if (initial_rest) {
    // MayBegin always has a zero delta.
    [events addObject:cocoa_test_event_utils::TestScrollEvent(
        Flip(default_location_).ToCGPoint(), test_window(), delta_x, 0, true,
        NSEventPhaseMayBegin, NSEventPhaseNone)];
    if (delta_y == 0) {
      // Rest and release: event gets cancelled.
      DCHECK_EQ(0, momentum_delta_y);  // Pretty sure this is impossible.
      [events addObject:cocoa_test_event_utils::TestScrollEvent(
          Flip(default_location_).ToCGPoint(), test_window(), delta_x, 0, true,
          NSEventPhaseCancelled, NSEventPhaseNone)];
      return events.autorelease();
    }
  }

  // With or without a rest, a begin is sent. It can have a non-zero
  // deviceDeltaY but regular deltaY is always 0.
  [events addObject:cocoa_test_event_utils::TestScrollEvent(
      Flip(default_location_).ToCGPoint(), test_window(), delta_x, 0, true,
      NSEventPhaseBegan, NSEventPhaseNone)];

  [events addObject:cocoa_test_event_utils::TestScrollEvent(
      Flip(default_location_).ToCGPoint(), test_window(), delta_x, delta_y,
      true, NSEventPhaseChanged, NSEventPhaseNone)];

  // With or without momentum, an end is sent for the non-momentum part.
  [events addObject:cocoa_test_event_utils::TestScrollEvent(
      Flip(default_location_).ToCGPoint(), test_window(), delta_x, 0, true,
      NSEventPhaseEnded, NSEventPhaseNone)];

  if (momentum_delta_y == 0)
    return events.autorelease();

  // Flick part. Basically the same, but with phase and momentumPhase swapped.
  [events addObject:cocoa_test_event_utils::TestScrollEvent(
      Flip(default_location_).ToCGPoint(), test_window(), delta_x, 0, true,
      NSEventPhaseNone, NSEventPhaseBegan)];

  [events addObject:cocoa_test_event_utils::TestScrollEvent(
      Flip(default_location_).ToCGPoint(), test_window(), delta_x,
      momentum_delta_y, true, NSEventPhaseNone, NSEventPhaseChanged)];

  [events addObject:cocoa_test_event_utils::TestScrollEvent(
      Flip(default_location_).ToCGPoint(), test_window(), delta_x, 0, true,
      NSEventPhaseNone, NSEventPhaseEnded)];
  return events.autorelease();
}

}  // namespace

TEST_F(EventsMacTest, EventFlagsFromNative) {
  // Left click.
  NSEvent* left = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp, 0);
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON, EventFlagsFromNative(left));

  // Right click.
  NSEvent* right = cocoa_test_event_utils::MouseEventWithType(NSRightMouseUp,
                                                              0);
  EXPECT_EQ(EF_RIGHT_MOUSE_BUTTON, EventFlagsFromNative(right));

  // Middle click.
  NSEvent* middle = cocoa_test_event_utils::MouseEventWithType(NSOtherMouseUp,
                                                               0);
  EXPECT_EQ(EF_MIDDLE_MOUSE_BUTTON, EventFlagsFromNative(middle));

  // Caps + Left
  NSEvent* caps = cocoa_test_event_utils::MouseEventWithType(
      NSLeftMouseUp, NSAlphaShiftKeyMask);
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON | EF_CAPS_LOCK_ON, EventFlagsFromNative(caps));

  // Shift + Left
  NSEvent* shift = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                              NSShiftKeyMask);
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON | EF_SHIFT_DOWN, EventFlagsFromNative(shift));

  // Ctrl + Left. Note we map this to a right click on Mac and remove Control.
  NSEvent* ctrl = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                             NSControlKeyMask);
  EXPECT_EQ(EF_RIGHT_MOUSE_BUTTON, EventFlagsFromNative(ctrl));

  // Ctrl + Right. Remains a right click.
  NSEvent* ctrl_right = cocoa_test_event_utils::MouseEventWithType(
      NSRightMouseUp, NSControlKeyMask);
  EXPECT_EQ(EF_RIGHT_MOUSE_BUTTON | EF_CONTROL_DOWN,
            EventFlagsFromNative(ctrl_right));

  // Alt + Left
  NSEvent* alt = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                            NSAlternateKeyMask);
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON | EF_ALT_DOWN, EventFlagsFromNative(alt));

  // Cmd + Left
  NSEvent* cmd = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp,
                                                            NSCommandKeyMask);
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON | EF_COMMAND_DOWN, EventFlagsFromNative(cmd));

  // Shift + Ctrl + Left. Also mapped to a right-click. Control removed.
  NSEvent* shiftctrl = cocoa_test_event_utils::MouseEventWithType(
      NSLeftMouseUp, NSShiftKeyMask | NSControlKeyMask);
  EXPECT_EQ(EF_RIGHT_MOUSE_BUTTON | EF_SHIFT_DOWN,
            EventFlagsFromNative(shiftctrl));

  // Cmd + Alt + Right
  NSEvent* cmdalt = cocoa_test_event_utils::MouseEventWithType(
      NSLeftMouseUp, NSCommandKeyMask | NSAlternateKeyMask);
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON | EF_COMMAND_DOWN | EF_ALT_DOWN,
            EventFlagsFromNative(cmdalt));
}

// Tests mouse button presses and mouse wheel events.
TEST_F(EventsMacTest, ButtonEvents) {
  gfx::Point location(5, 10);
  gfx::Vector2d offset;

  NSEvent* event =
      TestMouseEvent(kCGEventLeftMouseDown, location, kNoEventFlags);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, ui::EventTypeFromNative(event));
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, ui::EventFlagsFromNative(event));
  EXPECT_EQ(location, ui::EventLocationFromNative(event));
  EXPECT_EQ(ui::EventLocationFromNative(event),
            gfx::ToFlooredPoint(ui::EventLocationFromNativeF(event)));

  event =
      TestMouseEvent(kCGEventOtherMouseDown, location, kCGEventFlagMaskShift);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, ui::EventTypeFromNative(event));
  EXPECT_EQ(ui::EF_MIDDLE_MOUSE_BUTTON | ui::EF_SHIFT_DOWN,
            ui::EventFlagsFromNative(event));
  EXPECT_EQ(location, ui::EventLocationFromNative(event));
  EXPECT_EQ(ui::EventLocationFromNative(event),
            gfx::ToFlooredPoint(ui::EventLocationFromNativeF(event)));

  event = TestMouseEvent(kCGEventRightMouseUp, location, kNoEventFlags);
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, ui::EventTypeFromNative(event));
  EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, ui::EventFlagsFromNative(event));
  EXPECT_EQ(location, ui::EventLocationFromNative(event));
  EXPECT_EQ(ui::EventLocationFromNative(event),
            gfx::ToFlooredPoint(ui::EventLocationFromNativeF(event)));

  // Scroll up.
  event = TestScrollEvent(location, 0, 1);
  EXPECT_EQ(ui::ET_SCROLL, ui::EventTypeFromNative(event));
  EXPECT_EQ(0, ui::EventFlagsFromNative(event));
  EXPECT_EQ(location.ToString(), ui::EventLocationFromNative(event).ToString());
  EXPECT_EQ(ui::EventLocationFromNative(event),
            gfx::ToFlooredPoint(ui::EventLocationFromNativeF(event)));
  offset = ui::GetMouseWheelOffset(event);
  EXPECT_GT(offset.y(), 0);
  EXPECT_EQ(0, offset.x());

  // Scroll down.
  event = TestScrollEvent(location, 0, -1);
  EXPECT_EQ(ui::ET_SCROLL, ui::EventTypeFromNative(event));
  EXPECT_EQ(0, ui::EventFlagsFromNative(event));
  EXPECT_EQ(location, ui::EventLocationFromNative(event));
  EXPECT_EQ(ui::EventLocationFromNative(event),
            gfx::ToFlooredPoint(ui::EventLocationFromNativeF(event)));
  offset = ui::GetMouseWheelOffset(event);
  EXPECT_LT(offset.y(), 0);
  EXPECT_EQ(0, offset.x());

  // Scroll left.
  event = TestScrollEvent(location, 1, 0);
  EXPECT_EQ(ui::ET_SCROLL, ui::EventTypeFromNative(event));
  EXPECT_EQ(0, ui::EventFlagsFromNative(event));
  EXPECT_EQ(location, ui::EventLocationFromNative(event));
  EXPECT_EQ(ui::EventLocationFromNative(event),
            gfx::ToFlooredPoint(ui::EventLocationFromNativeF(event)));
  offset = ui::GetMouseWheelOffset(event);
  EXPECT_EQ(0, offset.y());
  EXPECT_GT(offset.x(), 0);

  // Scroll right.
  event = TestScrollEvent(location, -1, 0);
  EXPECT_EQ(ui::ET_SCROLL, ui::EventTypeFromNative(event));
  EXPECT_EQ(0, ui::EventFlagsFromNative(event));
  EXPECT_EQ(location, ui::EventLocationFromNative(event));
  EXPECT_EQ(ui::EventLocationFromNative(event),
            gfx::ToFlooredPoint(ui::EventLocationFromNativeF(event)));
  offset = ui::GetMouseWheelOffset(event);
  EXPECT_EQ(0, offset.y());
  EXPECT_LT(offset.x(), 0);
}

// Test correct location when the window has a native titlebar.
TEST_F(EventsMacTest, NativeTitlebarEventLocation) {
  gfx::Point location(5, 10);
  NSUInteger style_mask = NSTitledWindowMask | NSClosableWindowMask |
                          NSMiniaturizableWindowMask | NSResizableWindowMask;

  // First check that the window provided by ui::CocoaTest is how we think.
  DCHECK_EQ(NSBorderlessWindowMask, [test_window() styleMask]);
  [test_window() setStyleMask:style_mask];
  DCHECK_EQ(style_mask, [test_window() styleMask]);

  // EventLocationFromNative should behave the same as the ButtonEvents test.
  NSEvent* event =
      TestMouseEvent(kCGEventLeftMouseDown, location, kNoEventFlags);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, ui::EventTypeFromNative(event));
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, ui::EventFlagsFromNative(event));
  EXPECT_EQ(location, ui::EventLocationFromNative(event));

  // And be explicit, to ensure the test doesn't depend on some property of the
  // test harness. The change to the frame rect could be OS-specfic, so set it
  // to a known value.
  const CGFloat kTestHeight = 400;
  NSRect content_rect = NSMakeRect(0, 0, 600, kTestHeight);
  NSRect frame_rect = [test_window() frameRectForContentRect:content_rect];
  [test_window() setFrame:frame_rect display:YES];
  event = [NSEvent mouseEventWithType:NSLeftMouseDown
                             location:NSMakePoint(0, 0)  // Bottom-left corner.
                        modifierFlags:0
                            timestamp:0
                         windowNumber:[test_window() windowNumber]
                              context:nil
                          eventNumber:0
                           clickCount:0
                             pressure:1.0];
  // Bottom-left corner should be flipped.
  EXPECT_EQ(gfx::Point(0, kTestHeight), ui::EventLocationFromNative(event));

  // Removing the border, and sending the same event should move it down in the
  // toolkit-views coordinate system.
  int height_change = NSHeight(frame_rect) - kTestHeight;
  EXPECT_GT(height_change, 0);
  [test_window() setStyleMask:NSBorderlessWindowMask];
  [test_window() setFrame:frame_rect display:YES];
  EXPECT_EQ(gfx::Point(0, kTestHeight + height_change),
            ui::EventLocationFromNative(event));
}

// Testing for ui::EventTypeFromNative() not covered by ButtonEvents.
TEST_F(EventsMacTest, EventTypeFromNative) {
  NSEvent* event = cocoa_test_event_utils::KeyEventWithType(NSKeyDown, 0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, ui::EventTypeFromNative(event));

  event = cocoa_test_event_utils::KeyEventWithType(NSKeyUp, 0);
  EXPECT_EQ(ui::ET_KEY_RELEASED, ui::EventTypeFromNative(event));

  event = cocoa_test_event_utils::MouseEventWithType(NSLeftMouseDragged, 0);
  EXPECT_EQ(ui::ET_MOUSE_DRAGGED, ui::EventTypeFromNative(event));
  event = cocoa_test_event_utils::MouseEventWithType(NSRightMouseDragged, 0);
  EXPECT_EQ(ui::ET_MOUSE_DRAGGED, ui::EventTypeFromNative(event));
  event = cocoa_test_event_utils::MouseEventWithType(NSOtherMouseDragged, 0);
  EXPECT_EQ(ui::ET_MOUSE_DRAGGED, ui::EventTypeFromNative(event));

  event = cocoa_test_event_utils::MouseEventWithType(NSMouseMoved, 0);
  EXPECT_EQ(ui::ET_MOUSE_MOVED, ui::EventTypeFromNative(event));

  event = cocoa_test_event_utils::EnterEvent();
  EXPECT_EQ(ui::ET_MOUSE_ENTERED, ui::EventTypeFromNative(event));
  event = cocoa_test_event_utils::ExitEvent();
  EXPECT_EQ(ui::ET_MOUSE_EXITED, ui::EventTypeFromNative(event));
}

// Verify that a mouse wheel scroll event is correctly lacking phase data.
TEST_F(EventsMacTest, MouseWheelScroll) {
  int32_t wheel_delta_y = 2;

  NSEvent* ns_wheel = TestScrollEvent(default_location_, 0, wheel_delta_y);
  EXPECT_FALSE([ns_wheel hasPreciseScrollingDeltas]);
  ui::ScrollEvent wheel(ns_wheel);
  EXPECT_EQ(ui::ET_SCROLL, wheel.type());

  // Currently wheel events still say two for finger count, but this may change.
  EXPECT_EQ(2, wheel.finger_count());

  // Note the phase is "end" for wheel events, not "none". There is always an
  // "end" when no more events are expected.
  EXPECT_EQ(ui::EventMomentumPhase::END, wheel.momentum_phase());
  EXPECT_EQ(default_location_, wheel.location());

  float pixel_delta_y = wheel_delta_y * ui::kScrollbarPixelsPerCocoaTick;
  EXPECT_EQ(pixel_delta_y, wheel.y_offset_ordinal());
  EXPECT_EQ(0, wheel.x_offset_ordinal());
}

// Test the event flow for a trackpad "rest" that doesn't result in scrolling
// nor momentum. Also check the boring stuff like type, finger count and
// location, which isn't phase-specific.
// Sequence:
// (1) NSEvent: type=ScrollWheel loc=(780,41) time=14909.3 flags=0x100 win=<set>
//     {deviceD,d}elta{X,Y,Z}=0 count:0 phase=MayBegin momentumPhase=None
// (2) NSEvent: type=ScrollWheel loc=(780,41) time=14912.9 flags=0x100 win=<set>
//     {deviceD,d}elta{X,Y,Z}=0 count:0 phase=Cancelled momentumPhase=None.
TEST_F(EventsMacTest, TrackpadRestRelease) {
  NSArray* ns_events = TrackpadScrollSequence(true, 0, 0);
  ASSERT_EQ(2u, [ns_events count]);
  EXPECT_TRUE([ns_events[0] hasPreciseScrollingDeltas]);

  ui::ScrollEvent rest(ns_events[0]);
  EXPECT_EQ(ui::ET_SCROLL, rest.type());
  EXPECT_EQ(2, rest.finger_count());
  EXPECT_EQ(ui::EventMomentumPhase::MAY_BEGIN, rest.momentum_phase());
  EXPECT_EQ(0, rest.y_offset_ordinal());
  EXPECT_EQ(default_location_, rest.location());

  ui::ScrollEvent cancel(ns_events[1]);
  EXPECT_EQ(ui::ET_SCROLL, cancel.type());
  EXPECT_EQ(2, cancel.finger_count());
  EXPECT_EQ(ui::EventMomentumPhase::END, cancel.momentum_phase());
  EXPECT_EQ(0, cancel.y_offset_ordinal());
  EXPECT_EQ(default_location_, cancel.location());
}

// Test the event flow for touching the trackpad while "in motion" already, then
// pausing so that a flick is not generated. deltaX and deltaZ are always zero.
// Note, deviceDeltaX may take on an integer value even though deltaX is zero.
// Sequence:
// (1) NSEvent: type=ScrollWheel loc=(780,41) time=15263.2 flags=0x100 win=<set>
//     deltaY=0.000000 deviceDeltaY=1.000000 phase=Began momentumPhase=None
// (n) NSEvent: type=ScrollWheel loc=(780,41) time=15263.2 flags=0x100 win=<set>
//     deltaY=0.400024 deviceDeltaY=3.000000 phase=Changed momentumPhase=None
// (3) NSEvent: type=ScrollWheel loc=(780,41) time=15264.2 flags=0x100 win=<set>
//     deltaY=0.000000 deviceDeltaY=0.000000 phase=Ended momentumPhase=None.
TEST_F(EventsMacTest, TrackpadScrollThenRest) {
  int32_t delta_y = 21;

  NSArray* ns_events = TrackpadScrollSequence(false, delta_y, 0);
  ASSERT_EQ(3u, [ns_events count]);

  ui::ScrollEvent begin(ns_events[0]);
  EXPECT_EQ(ui::EventMomentumPhase::MAY_BEGIN, begin.momentum_phase());
  EXPECT_EQ(0, begin.y_offset_ordinal());

  ui::ScrollEvent update(ns_events[1]);
  // There's no momentum yet, so phase is none.
  EXPECT_EQ(ui::EventMomentumPhase::NONE, update.momentum_phase());
  // Note: No pixel conversion for "precise" deltas.
  EXPECT_EQ(delta_y, update.y_offset_ordinal());

  ui::ScrollEvent end(ns_events[2]);
  EXPECT_EQ(ui::EventMomentumPhase::END, end.momentum_phase());
  EXPECT_EQ(0, end.y_offset_ordinal());
}

// Same as the above, but with an initial rest, which is not cancelled. This
// results in multiple MAY_BEGIN phases.
TEST_F(EventsMacTest, TrackpadRestThenScrollThenRest) {
  int32_t delta_y = 21;

  NSArray* ns_events = TrackpadScrollSequence(true, delta_y, 0);
  ASSERT_EQ(4u, [ns_events count]);

  ui::ScrollEvent rest(ns_events[0]);
  EXPECT_EQ(ui::EventMomentumPhase::MAY_BEGIN, rest.momentum_phase());
  EXPECT_EQ(0, rest.y_offset_ordinal());

  ui::ScrollEvent begin(ns_events[1]);
  EXPECT_EQ(ui::EventMomentumPhase::MAY_BEGIN, begin.momentum_phase());
  EXPECT_EQ(0, begin.y_offset_ordinal());

  ui::ScrollEvent update(ns_events[2]);
  EXPECT_EQ(ui::EventMomentumPhase::NONE, update.momentum_phase());
  EXPECT_EQ(delta_y, update.y_offset_ordinal());

  ui::ScrollEvent end(ns_events[3]);
  EXPECT_EQ(ui::EventMomentumPhase::END, end.momentum_phase());
  EXPECT_EQ(0, end.y_offset_ordinal());
}

// Test the event flows that lead to momentum, with and without an initial rest.
// Example sequence (no initial rest):
// (1) NSEvent: type=ScrollWheel loc=(780,41) time=15187.5 flags=0x100 win=<set>
//     deltaY=0.000000 deviceDeltaY=1.000000 phase=Began momentumPhase=None
// (n) NSEvent: type=ScrollWheel loc=(780,41) time=15187.5 flags=0x100 win=<set>
//     deltaY=0.500031  deviceDeltaY=4.000000 phase=Changed momentumPhase=None
// (3) NSEvent: type=ScrollWheel loc=(780,41) time=15187.6 flags=0x100 win=<set>
//     deltaY=0.000000 deviceDeltaY=0.000000 phase=Ended momentumPhase=None
// (4) NSEvent: type=ScrollWheel loc=(780,41) time=15187.6 flags=0x100 win=<set>
//     deltaY=0.900055 deviceDeltaY=3.000000 phase=None momentumPhase=Began
// (n) NSEvent: type=ScrollWheel loc=(780,41) time=15187.6 flags=0x100 win=<set>
//     deltaY=0.300018 deviceDeltaY=3.000000 phase=None momentumPhase=Changed
// (6) NSEvent: type=ScrollWheel loc=(780,41) time=15188.0 flags=0x100 win=<set>
//     deltaY=0.000000 deviceDeltaY=0.000000 phase=None momentumPhase=Ended.
TEST_F(EventsMacTest, TrackpadScrollThenFlick) {
  int32_t delta_y = 21;
  int32_t momentum_delta_y = 33;

  NSArray* ns_events = TrackpadScrollSequence(false, delta_y, momentum_delta_y);
  ASSERT_EQ(6u, [ns_events count]);

  // Non-momentum part.
  {
    ui::ScrollEvent begin(ns_events[0]);
    EXPECT_EQ(ui::EventMomentumPhase::MAY_BEGIN, begin.momentum_phase());
    EXPECT_EQ(0, begin.y_offset_ordinal());

    ui::ScrollEvent update(ns_events[1]);
    EXPECT_EQ(ui::EventMomentumPhase::NONE, update.momentum_phase());
    EXPECT_EQ(delta_y, update.y_offset_ordinal());

    ui::ScrollEvent end(ns_events[2]);
    // Even though the event stream continues, AppKit doesn't provide a way to
    // know this without peeking at future events. So this "end" mid-stream is
    // unavoidable.
    EXPECT_EQ(ui::EventMomentumPhase::END, end.momentum_phase());
    EXPECT_EQ(0, end.y_offset_ordinal());
  }
  // Momentum part.
  {
    ui::ScrollEvent begin(ns_events[3]);
    // Since a momentum "begin" is really a continuation of the stream, it's
    // currently treated as an update, but the offsets should always be zero.
    EXPECT_EQ(ui::EventMomentumPhase::INERTIAL_UPDATE, begin.momentum_phase());
    EXPECT_EQ(0, begin.y_offset_ordinal());

    ui::ScrollEvent update(ns_events[4]);
    EXPECT_EQ(ui::EventMomentumPhase::INERTIAL_UPDATE, update.momentum_phase());
    EXPECT_EQ(momentum_delta_y, update.y_offset_ordinal());

    ui::ScrollEvent end(ns_events[5]);
    EXPECT_EQ(ui::EventMomentumPhase::END, end.momentum_phase());
    EXPECT_EQ(0, end.y_offset_ordinal());
  }
}

// Check that NSFlagsChanged event is translated to key press or release event.
TEST_F(EventsMacTest, HandleModifierOnlyKeyEvents) {
  struct {
    const char* description;
    NSEventModifierFlags modifier_flags;
    uint16_t key_code;
    EventType expected_type;
    KeyboardCode expected_key_code;
  } test_cases[] = {
      {"CapsLock pressed", NSAlphaShiftKeyMask, kVK_CapsLock, ET_KEY_PRESSED,
       VKEY_CAPITAL},
      {"CapsLock released", 0, kVK_CapsLock, ET_KEY_RELEASED, VKEY_CAPITAL},
      {"Shift pressed", NSShiftKeyMask, kVK_Shift, ET_KEY_PRESSED, VKEY_SHIFT},
      {"Shift released", 0, kVK_Shift, ET_KEY_RELEASED, VKEY_SHIFT},
      {"Control pressed", NSControlKeyMask, kVK_Control, ET_KEY_PRESSED,
       VKEY_CONTROL},
      {"Control released", 0, kVK_Control, ET_KEY_RELEASED, VKEY_CONTROL},
      {"Option pressed", NSAlternateKeyMask, kVK_Option, ET_KEY_PRESSED,
       VKEY_MENU},
      {"Option released", 0, kVK_Option, ET_KEY_RELEASED, VKEY_MENU},
      {"Command pressed", NSCommandKeyMask, kVK_Command, ET_KEY_PRESSED,
       VKEY_LWIN},
      {"Command released", 0, kVK_Command, ET_KEY_RELEASED, VKEY_LWIN},
      {"Shift pressed with CapsLock on", NSShiftKeyMask | NSAlphaShiftKeyMask,
       kVK_Shift, ET_KEY_PRESSED, VKEY_SHIFT},
      {"Shift released with CapsLock off", NSAlphaShiftKeyMask, kVK_Shift,
       ET_KEY_RELEASED, VKEY_SHIFT},
  };
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(::testing::Message() << "While checking case: "
                                      << test_case.description);
    NSEvent* native_event = cocoa_test_event_utils::KeyEventWithModifierOnly(
        test_case.key_code, test_case.modifier_flags);
    std::unique_ptr<ui::Event> event = EventFromNative(native_event);
    EXPECT_TRUE(event);
    EXPECT_EQ(test_case.expected_type, event->type());
    EXPECT_EQ(test_case.expected_key_code, event->AsKeyEvent()->key_code());
  }
}

}  // namespace ui
