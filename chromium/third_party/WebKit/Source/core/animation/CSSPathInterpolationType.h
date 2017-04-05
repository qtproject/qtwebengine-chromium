// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPathInterpolationType_h
#define CSSPathInterpolationType_h

#include "core/animation/CSSInterpolationType.h"

namespace blink {

class CSSPathInterpolationType : public CSSInterpolationType {
 public:
  CSSPathInterpolationType(PropertyHandle property)
      : CSSInterpolationType(property) {}

  void applyStandardPropertyValue(const InterpolableValue&,
                                  const NonInterpolableValue*,
                                  StyleResolverState&) const final;
  void composite(UnderlyingValueOwner&,
                 double underlyingFraction,
                 const InterpolationValue&,
                 double interpolationFraction) const final;

 protected:
  InterpolationValue maybeConvertNeutral(const InterpolationValue& underlying,
                                         ConversionCheckers&) const final;
  InterpolationValue maybeConvertInitial(const StyleResolverState&,
                                         ConversionCheckers&) const final;
  InterpolationValue maybeConvertInherit(const StyleResolverState&,
                                         ConversionCheckers&) const final;
  InterpolationValue maybeConvertValue(const CSSValue&,
                                       const StyleResolverState&,
                                       ConversionCheckers&) const final;
  InterpolationValue maybeConvertStandardPropertyUnderlyingValue(
      const StyleResolverState&) const final;
  PairwiseInterpolationValue maybeMergeSingles(
      InterpolationValue&& start,
      InterpolationValue&& end) const final;
};

}  // namespace blink

#endif  // CSSPathInterpolationType_h
