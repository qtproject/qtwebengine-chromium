// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/memory_instrumentation/public/cpp/memory_instrumentation_traits.h"

#include "base/trace_event/memory_dump_request_args.h"
#include "services/memory_instrumentation/public/interfaces/memory_instrumentation.mojom.h"

namespace mojo {

//static
memory_instrumentation::mojom::DumpType
EnumTraits<memory_instrumentation::mojom::DumpType,
           base::trace_event::MemoryDumpType>::ToMojom(
    base::trace_event::MemoryDumpType type) {
  switch (type) {
    case base::trace_event::MemoryDumpType::PERIODIC_INTERVAL:
      return memory_instrumentation::mojom::DumpType::PERIODIC_INTERVAL;
    case base::trace_event::MemoryDumpType::EXPLICITLY_TRIGGERED:
      return memory_instrumentation::mojom::DumpType::EXPLICITLY_TRIGGERED;
    case base::trace_event::MemoryDumpType::PEAK_MEMORY_USAGE:
      return memory_instrumentation::mojom::DumpType::PEAK_MEMORY_USAGE;
    default:
      CHECK(false) << "Invalid type: " << static_cast<uint8_t>(type);
      // This should not be reached. Just return a random value.
      return memory_instrumentation::mojom::DumpType::PEAK_MEMORY_USAGE;
  }
}

//static
bool EnumTraits<memory_instrumentation::mojom::DumpType,
               base::trace_event::MemoryDumpType>::FromMojom(
    memory_instrumentation::mojom::DumpType input,
    base::trace_event::MemoryDumpType* out) {
  switch (input) {
    case memory_instrumentation::mojom::DumpType::PERIODIC_INTERVAL:
      *out = base::trace_event::MemoryDumpType::PERIODIC_INTERVAL;
      break;
    case memory_instrumentation::mojom::DumpType::EXPLICITLY_TRIGGERED:
      *out = base::trace_event::MemoryDumpType::EXPLICITLY_TRIGGERED;
      break;
    case memory_instrumentation::mojom::DumpType::PEAK_MEMORY_USAGE:
      *out = base::trace_event::MemoryDumpType::PEAK_MEMORY_USAGE;
      break;
    default:
      NOTREACHED() << "Invalid type: " << static_cast<uint8_t>(input);
      return false;
  }
  return true;
}

//static
memory_instrumentation::mojom::LevelOfDetail
EnumTraits<memory_instrumentation::mojom::LevelOfDetail,
           base::trace_event::MemoryDumpLevelOfDetail>::ToMojom(
    base::trace_event::MemoryDumpLevelOfDetail level_of_detail) {
  switch (level_of_detail) {
    case base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND:
      return memory_instrumentation::mojom::LevelOfDetail::BACKGROUND;
    case base::trace_event::MemoryDumpLevelOfDetail::LIGHT:
      return memory_instrumentation::mojom::LevelOfDetail::LIGHT;
    case base::trace_event::MemoryDumpLevelOfDetail::DETAILED:
      return memory_instrumentation::mojom::LevelOfDetail::DETAILED;
    default:
      CHECK(false) << "Invalid type: " << static_cast<uint8_t>(level_of_detail);
      // This should not be reached. Just return a random value.
      return memory_instrumentation::mojom::LevelOfDetail::BACKGROUND;
  }
}

//static
bool EnumTraits<memory_instrumentation::mojom::LevelOfDetail,
               base::trace_event::MemoryDumpLevelOfDetail>::FromMojom(
    memory_instrumentation::mojom::LevelOfDetail input,
    base::trace_event::MemoryDumpLevelOfDetail* out) {
  switch (input) {
    case memory_instrumentation::mojom::LevelOfDetail::BACKGROUND:
      *out = base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND;
      break;
    case memory_instrumentation::mojom::LevelOfDetail::LIGHT:
      *out = base::trace_event::MemoryDumpLevelOfDetail::LIGHT;
      break;
    case memory_instrumentation::mojom::LevelOfDetail::DETAILED:
      *out = base::trace_event::MemoryDumpLevelOfDetail::DETAILED;
      break;
    default:
      NOTREACHED() << "Invalid type: " << static_cast<uint8_t>(input);
      return false;
  }
  return true;
}

//static
bool StructTraits<memory_instrumentation::mojom::RequestArgsDataView,
                  base::trace_event::MemoryDumpRequestArgs>::Read(
    memory_instrumentation::mojom::RequestArgsDataView input,
    base::trace_event::MemoryDumpRequestArgs* out) {
  out->dump_guid = input.dump_guid();
  if (!input.ReadDumpType(&out->dump_type))
    return false;
  if (!input.ReadLevelOfDetail(&out->level_of_detail))
    return false;
  return true;
}

}  // namespace mojo
