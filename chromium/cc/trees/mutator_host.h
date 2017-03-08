// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_MUTATOR_HOST_H_
#define CC_TREES_MUTATOR_HOST_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "cc/trees/element_id.h"
#include "cc/trees/mutator_host_client.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace gfx {
class ScrollOffset;
}

namespace cc {

class MutatorEvents;
class MutatorHostClient;

// A MutatorHost owns all the animation and mutation effects.
// There is just one MutatorHost for LayerTreeHost on main renderer thread
// and just one MutatorHost for LayerTreeHostImpl on the impl thread.
// We synchronize them during the commit in a one-way data-flow process
// (PushPropertiesTo).
// A MutatorHost talks to its correspondent LayerTreeHost via
// MutatorHostClient interface.
class MutatorHost {
 public:
  virtual ~MutatorHost() {}

  virtual std::unique_ptr<MutatorHost> CreateImplInstance(
      bool supports_impl_scrolling) const = 0;

  virtual void ClearMutators() = 0;

  virtual void RegisterElement(ElementId element_id,
                               ElementListType list_type) = 0;
  virtual void UnregisterElement(ElementId element_id,
                                 ElementListType list_type) = 0;

  virtual void SetMutatorHostClient(MutatorHostClient* client) = 0;

  virtual void PushPropertiesTo(MutatorHost* host_impl) = 0;

  virtual void SetSupportsScrollAnimations(bool supports_scroll_animations) = 0;
  virtual bool NeedsAnimateLayers() const = 0;

  virtual bool ActivateAnimations() = 0;
  virtual bool AnimateLayers(base::TimeTicks monotonic_time) = 0;
  virtual bool UpdateAnimationState(bool start_ready_animations,
                                    MutatorEvents* events) = 0;

  virtual std::unique_ptr<MutatorEvents> CreateEvents() = 0;
  virtual void SetAnimationEvents(std::unique_ptr<MutatorEvents> events) = 0;

  virtual bool ScrollOffsetAnimationWasInterrupted(
      ElementId element_id) const = 0;

  virtual bool IsAnimatingFilterProperty(ElementId element_id,
                                         ElementListType list_type) const = 0;
  virtual bool IsAnimatingOpacityProperty(ElementId element_id,
                                          ElementListType list_type) const = 0;
  virtual bool IsAnimatingTransformProperty(
      ElementId element_id,
      ElementListType list_type) const = 0;

  virtual bool HasPotentiallyRunningFilterAnimation(
      ElementId element_id,
      ElementListType list_type) const = 0;
  virtual bool HasPotentiallyRunningOpacityAnimation(
      ElementId element_id,
      ElementListType list_type) const = 0;
  virtual bool HasPotentiallyRunningTransformAnimation(
      ElementId element_id,
      ElementListType list_type) const = 0;

  virtual bool HasAnyAnimationTargetingProperty(
      ElementId element_id,
      TargetProperty::Type property) const = 0;

  virtual bool HasFilterAnimationThatInflatesBounds(
      ElementId element_id) const = 0;
  virtual bool HasTransformAnimationThatInflatesBounds(
      ElementId element_id) const = 0;
  virtual bool HasAnimationThatInflatesBounds(ElementId element_id) const = 0;

  virtual bool FilterAnimationBoundsForBox(ElementId element_id,
                                           const gfx::BoxF& box,
                                           gfx::BoxF* bounds) const = 0;
  virtual bool TransformAnimationBoundsForBox(ElementId element_id,
                                              const gfx::BoxF& box,
                                              gfx::BoxF* bounds) const = 0;

  virtual bool HasOnlyTranslationTransforms(
      ElementId element_id,
      ElementListType list_type) const = 0;
  virtual bool AnimationsPreserveAxisAlignment(ElementId element_id) const = 0;

  virtual bool MaximumTargetScale(ElementId element_id,
                                  ElementListType list_type,
                                  float* max_scale) const = 0;
  virtual bool AnimationStartScale(ElementId element_id,
                                   ElementListType list_type,
                                   float* start_scale) const = 0;

  virtual bool HasAnyAnimation(ElementId element_id) const = 0;
  virtual bool HasActiveAnimationForTesting(ElementId element_id) const = 0;

  virtual void ImplOnlyScrollAnimationCreate(
      ElementId element_id,
      const gfx::ScrollOffset& target_offset,
      const gfx::ScrollOffset& current_offset,
      base::TimeDelta delayed_by) = 0;
  virtual bool ImplOnlyScrollAnimationUpdateTarget(
      ElementId element_id,
      const gfx::Vector2dF& scroll_delta,
      const gfx::ScrollOffset& max_scroll_offset,
      base::TimeTicks frame_monotonic_time,
      base::TimeDelta delayed_by) = 0;

  virtual void ScrollAnimationAbort(bool needs_completion) = 0;
};

class MutatorEvents {
 public:
  virtual ~MutatorEvents() {}
  virtual bool IsEmpty() const = 0;
};

}  // namespace cc

#endif  // CC_TREES_MUTATOR_HOST_H_
