// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DISCARDABLE_MEMORY_SERVICE_DISCARDABLE_SHARED_MEMORY_MANAGER_H_
#define COMPONENTS_DISCARDABLE_MEMORY_SERVICE_DISCARDABLE_SHARED_MEMORY_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/format_macros.h"
#include "base/macros.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/memory/discardable_shared_memory.h"
#include "base/memory/memory_coordinator_client.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/discardable_memory/common/discardable_memory_export.h"
#include "components/discardable_memory/common/discardable_shared_memory_id.h"

namespace discardable_memory {

// Implementation of DiscardableMemoryAllocator that allocates and manages
// discardable memory segments for the process which hosts this class, and
// for remote processes which request discardable memory from this class via
// IPC.
// This class is thread-safe and instances can safely be used on any thread.
class DISCARDABLE_MEMORY_EXPORT DiscardableSharedMemoryManager
    : public base::DiscardableMemoryAllocator,
      public base::trace_event::MemoryDumpProvider,
      public base::MemoryCoordinatorClient {
 public:
  DiscardableSharedMemoryManager();
  ~DiscardableSharedMemoryManager() override;

  // Returns a singleton instance.
  static DiscardableSharedMemoryManager* current();

  // Overridden from base::DiscardableMemoryAllocator:
  std::unique_ptr<base::DiscardableMemory> AllocateLockedDiscardableMemory(
      size_t size) override;

  // Overridden from base::trace_event::MemoryDumpProvider:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // TODO(penghuang): Get ride of the |process_handle| when we switch to mojo.
  // This allocates a discardable memory segment for |process_handle|.
  // A valid shared memory handle is returned on success.
  void AllocateLockedDiscardableSharedMemoryForClient(
      base::ProcessHandle process_handle,
      int client_id,
      size_t size,
      DiscardableSharedMemoryId id,
      base::SharedMemoryHandle* shared_memory_handle);

  // Call this to notify the manager that client process associated with
  // |client_id| has deleted discardable memory segment with |id|.
  void ClientDeletedDiscardableSharedMemory(DiscardableSharedMemoryId id,
                                            int client_id);

  // Call this to notify the manager that client associated with |client_id|
  // has been removed. The manager will use this to release memory segments
  // allocated for client to the OS.
  void ClientRemoved(int client_id);

  // The maximum number of bytes of memory that may be allocated. This will
  // cause memory usage to be reduced if currently above |limit|.
  void SetMemoryLimit(size_t limit);

  // Reduce memory usage if above current memory limit.
  void EnforceMemoryPolicy();

  // Returns bytes of allocated discardable memory.
  size_t GetBytesAllocated();

 private:
  class MemorySegment : public base::RefCountedThreadSafe<MemorySegment> {
   public:
    MemorySegment(std::unique_ptr<base::DiscardableSharedMemory> memory);

    base::DiscardableSharedMemory* memory() const { return memory_.get(); }

   private:
    friend class base::RefCountedThreadSafe<MemorySegment>;

    ~MemorySegment();

    std::unique_ptr<base::DiscardableSharedMemory> memory_;

    DISALLOW_COPY_AND_ASSIGN(MemorySegment);
  };

  static bool CompareMemoryUsageTime(const scoped_refptr<MemorySegment>& a,
                                     const scoped_refptr<MemorySegment>& b) {
    // In this system, LRU memory segment is evicted first.
    return a->memory()->last_known_usage() > b->memory()->last_known_usage();
  }

  // base::MemoryCoordinatorClient implementation:
  void OnMemoryStateChange(base::MemoryState state) override;

  // TODO(penghuang): Get ride of the |process_handle| when we switch to mojo.
  void AllocateLockedDiscardableSharedMemory(
      base::ProcessHandle process_handle,
      int client_id,
      size_t size,
      DiscardableSharedMemoryId id,
      base::SharedMemoryHandle* shared_memory_handle);
  void DeletedDiscardableSharedMemory(DiscardableSharedMemoryId id,
                                      int client_id);
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);
  void ReduceMemoryUsageUntilWithinMemoryLimit();
  void ReduceMemoryUsageUntilWithinLimit(size_t limit);
  void ReleaseMemory(base::DiscardableSharedMemory* memory);
  void BytesAllocatedChanged(size_t new_bytes_allocated) const;

  // Virtual for tests.
  virtual base::Time Now() const;
  virtual void ScheduleEnforceMemoryPolicy();

  base::Lock lock_;
  typedef base::hash_map<DiscardableSharedMemoryId,
                         scoped_refptr<MemorySegment>>
      MemorySegmentMap;
  typedef base::hash_map<int, MemorySegmentMap> ClientMap;
  ClientMap clients_;
  // Note: The elements in |segments_| are arranged in such a way that they form
  // a heap. The LRU memory segment always first.
  typedef std::vector<scoped_refptr<MemorySegment>> MemorySegmentVector;
  MemorySegmentVector segments_;
  size_t default_memory_limit_;
  size_t memory_limit_;
  size_t bytes_allocated_;
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;
  scoped_refptr<base::SingleThreadTaskRunner>
      enforce_memory_policy_task_runner_;
  base::Closure enforce_memory_policy_callback_;
  bool enforce_memory_policy_pending_;
  base::WeakPtrFactory<DiscardableSharedMemoryManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableSharedMemoryManager);
};

}  // namespace discardable_memory

#endif  // COMPONENTS_DISCARDABLE_MEMORY_SERVICE_DISCARDABLE_SHARED_MEMORY_MANAGER_H_
