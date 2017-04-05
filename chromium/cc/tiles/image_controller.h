// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_IMAGE_CONTROLLER_H_
#define CC_TILES_IMAGE_CONTROLLER_H_

#include <set>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/simple_thread.h"
#include "cc/base/cc_export.h"
#include "cc/base/unique_notifier.h"
#include "cc/playback/draw_image.h"
#include "cc/raster/tile_task.h"
#include "cc/tiles/image_decode_cache.h"

namespace cc {

class CC_EXPORT ImageController {
 public:
  using ImageDecodeRequestId = uint64_t;
  using ImageDecodedCallback = base::Callback<void(ImageDecodeRequestId)>;
  explicit ImageController(
      base::SequencedTaskRunner* origin_task_runner,
      scoped_refptr<base::SequencedTaskRunner> worker_task_runner);
  virtual ~ImageController();

  void SetImageDecodeCache(ImageDecodeCache* cache);
  void GetTasksForImagesAndRef(
      std::vector<DrawImage>* images,
      std::vector<scoped_refptr<TileTask>>* tasks,
      const ImageDecodeCache::TracingInfo& tracing_info);
  void UnrefImages(const std::vector<DrawImage>& images);
  void ReduceMemoryUsage();
  std::vector<scoped_refptr<TileTask>> SetPredecodeImages(
      std::vector<DrawImage> predecode_images,
      const ImageDecodeCache::TracingInfo& tracing_info);

  // Virtual for testing.
  virtual void UnlockImageDecode(ImageDecodeRequestId id);

  // This function requests that the given image be decoded and locked. Once the
  // callback has been issued, it is passed an ID, which should be used to
  // unlock this image. It is up to the caller to ensure that the image is later
  // unlocked using UnlockImageDecode.
  // Virtual for testing.
  virtual ImageDecodeRequestId QueueImageDecode(
      sk_sp<const SkImage> image,
      const ImageDecodedCallback& callback);

 private:
  struct ImageDecodeRequest {
    ImageDecodeRequest();
    ImageDecodeRequest(ImageDecodeRequestId id,
                       const DrawImage& draw_image,
                       const ImageDecodedCallback& callback,
                       scoped_refptr<TileTask> task,
                       bool need_unref);
    ImageDecodeRequest(ImageDecodeRequest&& other);
    ImageDecodeRequest(const ImageDecodeRequest& other);
    ~ImageDecodeRequest();

    ImageDecodeRequest& operator=(ImageDecodeRequest&& other);
    ImageDecodeRequest& operator=(const ImageDecodeRequest& other);

    ImageDecodeRequestId id;
    DrawImage draw_image;
    ImageDecodedCallback callback;
    scoped_refptr<TileTask> task;
    bool need_unref;
  };

  void StopWorkerTasks();

  // Called from the worker thread.
  void ProcessNextImageDecodeOnWorkerThread();

  void ImageDecodeCompleted(ImageDecodeRequestId id);

  ImageDecodeCache* cache_ = nullptr;
  std::vector<DrawImage> predecode_locked_images_;

  static ImageDecodeRequestId s_next_image_decode_queue_id_;
  std::unordered_map<ImageDecodeRequestId, DrawImage> requested_locked_images_;

  base::SequencedTaskRunner* origin_task_runner_ = nullptr;
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;

  // The variables defined below this lock (aside from weak_ptr_factory_) can
  // only be accessed when the lock is acquired.
  base::Lock lock_;
  std::map<ImageDecodeRequestId, ImageDecodeRequest> image_decode_queue_;
  std::map<ImageDecodeRequestId, ImageDecodeRequest>
      requests_needing_completion_;
  bool abort_tasks_ = false;

  base::WeakPtrFactory<ImageController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImageController);
};

}  // namespace cc

#endif  // CC_TILES_IMAGE_CONTROLLER_H_
