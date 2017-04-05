// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webmediaplayer_ms_compositor.h"

#include <stdint.h>
#include <string>

#include "base/command_line.h"
#include "base/hash.h"
#include "base/single_thread_task_runner.h"
#include "base/values.h"
#include "content/renderer/media/webmediaplayer_ms.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/filters/video_renderer_algorithm.h"
#include "media/renderers/skcanvas_video_renderer.h"
#include "services/ui/public/cpp/gpu/context_provider_command_buffer.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "third_party/libyuv/include/libyuv/video_common.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace content {

namespace {

// This function copies |frame| to a new I420 or YV12A media::VideoFrame.
scoped_refptr<media::VideoFrame> CopyFrame(
    const scoped_refptr<media::VideoFrame>& frame,
    media::SkCanvasVideoRenderer* video_renderer) {
  scoped_refptr<media::VideoFrame> new_frame;
  if (frame->HasTextures()) {
    DCHECK(frame->format() == media::PIXEL_FORMAT_ARGB ||
           frame->format() == media::PIXEL_FORMAT_XRGB ||
           frame->format() == media::PIXEL_FORMAT_I420 ||
           frame->format() == media::PIXEL_FORMAT_UYVY ||
           frame->format() == media::PIXEL_FORMAT_NV12);
    new_frame = media::VideoFrame::CreateFrame(
        media::PIXEL_FORMAT_I420, frame->coded_size(), frame->visible_rect(),
        frame->natural_size(), frame->timestamp());

    sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(
        frame->visible_rect().width(), frame->visible_rect().height());

    ui::ContextProviderCommandBuffer* const provider =
        RenderThreadImpl::current()->SharedMainThreadContextProvider().get();
    if (surface && provider) {
      DCHECK(provider->ContextGL());
      video_renderer->Copy(
          frame.get(), surface->getCanvas(),
          media::Context3D(provider->ContextGL(), provider->GrContext()));
    } else {
      // Return a black frame (yuv = {0, 0x80, 0x80}).
      return media::VideoFrame::CreateColorFrame(
          frame->visible_rect().size(), 0u, 0x80, 0x80, frame->timestamp());
    }

    SkPixmap pixmap;
    const bool result = surface->getCanvas()->peekPixels(&pixmap);
    DCHECK(result) << "Error trying to access SkSurface's pixels";

    const uint32 source_pixel_format =
        (kN32_SkColorType == kRGBA_8888_SkColorType) ? libyuv::FOURCC_ABGR
                                                     : libyuv::FOURCC_ARGB;
    libyuv::ConvertToI420(
        static_cast<const uint8*>(pixmap.addr(0, 0)),
        pixmap.getSafeSize64(),
        new_frame->visible_data(media::VideoFrame::kYPlane),
        new_frame->stride(media::VideoFrame::kYPlane),
        new_frame->visible_data(media::VideoFrame::kUPlane),
        new_frame->stride(media::VideoFrame::kUPlane),
        new_frame->visible_data(media::VideoFrame::kVPlane),
        new_frame->stride(media::VideoFrame::kVPlane),
        0 /* crop_x */, 0 /* crop_y */,
        pixmap.width(), pixmap.height(),
        new_frame->visible_rect().width(), new_frame->visible_rect().height(),
        libyuv::kRotate0, source_pixel_format);
  } else {
    DCHECK(frame->IsMappable());
    DCHECK(frame->format() == media::PIXEL_FORMAT_YV12 ||
           frame->format() == media::PIXEL_FORMAT_YV12A ||
           frame->format() == media::PIXEL_FORMAT_I420);
    const gfx::Size& coded_size = frame->coded_size();
    new_frame = media::VideoFrame::CreateFrame(
        media::IsOpaque(frame->format()) ? media::PIXEL_FORMAT_I420
                                         : media::PIXEL_FORMAT_YV12A,
        coded_size, frame->visible_rect(), frame->natural_size(),
        frame->timestamp());
    libyuv::I420Copy(frame->data(media::VideoFrame::kYPlane),
                     frame->stride(media::VideoFrame::kYPlane),
                     frame->data(media::VideoFrame::kUPlane),
                     frame->stride(media::VideoFrame::kUPlane),
                     frame->data(media::VideoFrame::kVPlane),
                     frame->stride(media::VideoFrame::kVPlane),
                     new_frame->data(media::VideoFrame::kYPlane),
                     new_frame->stride(media::VideoFrame::kYPlane),
                     new_frame->data(media::VideoFrame::kUPlane),
                     new_frame->stride(media::VideoFrame::kUPlane),
                     new_frame->data(media::VideoFrame::kVPlane),
                     new_frame->stride(media::VideoFrame::kVPlane),
                     coded_size.width(), coded_size.height());
    if (frame->format() == media::PIXEL_FORMAT_YV12A) {
      libyuv::CopyPlane(frame->data(media::VideoFrame::kAPlane),
                        frame->stride(media::VideoFrame::kAPlane),
                        new_frame->data(media::VideoFrame::kAPlane),
                        new_frame->stride(media::VideoFrame::kAPlane),
                        coded_size.width(), coded_size.height());
    }
  }

  // Transfer metadata keys.
  new_frame->metadata()->MergeMetadataFrom(frame->metadata());
  return new_frame;
}

}  // anonymous namespace

WebMediaPlayerMSCompositor::WebMediaPlayerMSCompositor(
    const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
    const blink::WebMediaStream& web_stream,
    const base::WeakPtr<WebMediaPlayerMS>& player)
    : compositor_task_runner_(compositor_task_runner),
      player_(player),
      video_frame_provider_client_(nullptr),
      current_frame_used_by_compositor_(false),
      last_render_length_(base::TimeDelta::FromSecondsD(1.0 / 60.0)),
      total_frame_count_(0),
      dropped_frame_count_(0),
      stopped_(true) {
  main_message_loop_ = base::MessageLoop::current();
  io_thread_checker_.DetachFromThread();

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  if (!web_stream.isNull())
    web_stream.videoTracks(video_tracks);

  const bool remote_video =
      video_tracks.size() && video_tracks[0].source().remote();

  if (remote_video &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableRTCSmoothnessAlgorithm)) {
    base::AutoLock auto_lock(current_frame_lock_);
    rendering_frame_buffer_.reset(new media::VideoRendererAlgorithm(
        base::Bind(&WebMediaPlayerMSCompositor::MapTimestampsToRenderTimeTicks,
                   base::Unretained(this))));
  }

  // Just for logging purpose.
  std::string stream_id =
      web_stream.isNull() ? std::string() : web_stream.id().utf8();
  const uint32_t hash_value = base::Hash(stream_id);
  serial_ = (hash_value << 1) | (remote_video ? 1 : 0);
}

WebMediaPlayerMSCompositor::~WebMediaPlayerMSCompositor() {
  DCHECK(!video_frame_provider_client_)
      << "Must call StopUsingProvider() before dtor!";
}

gfx::Size WebMediaPlayerMSCompositor::GetCurrentSize() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(current_frame_lock_);
  return current_frame_ ? current_frame_->natural_size() : gfx::Size();
}

base::TimeDelta WebMediaPlayerMSCompositor::GetCurrentTime() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(current_frame_lock_);
  return current_frame_.get() ? current_frame_->timestamp() : base::TimeDelta();
}

size_t WebMediaPlayerMSCompositor::total_frame_count() const {
  DVLOG(1) << __func__ << ", " << total_frame_count_;
  DCHECK(thread_checker_.CalledOnValidThread());
  return total_frame_count_;
}

size_t WebMediaPlayerMSCompositor::dropped_frame_count() const {
  DVLOG(1) << __func__ << ", " << dropped_frame_count_;
  DCHECK(thread_checker_.CalledOnValidThread());
  return dropped_frame_count_;
}

void WebMediaPlayerMSCompositor::SetVideoFrameProviderClient(
    cc::VideoFrameProvider::Client* client) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  if (video_frame_provider_client_)
    video_frame_provider_client_->StopUsingProvider();

  video_frame_provider_client_ = client;
  if (video_frame_provider_client_ && !stopped_)
    video_frame_provider_client_->StartRendering();
}

void WebMediaPlayerMSCompositor::EnqueueFrame(
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(current_frame_lock_);
  ++total_frame_count_;

  // With algorithm off, just let |current_frame_| hold the incoming |frame|.
  if (!rendering_frame_buffer_) {
    SetCurrentFrame(frame);
    return;
  }

  // This is a signal frame saying that the stream is stopped.
  bool end_of_stream = false;
  if (frame->metadata()->GetBoolean(media::VideoFrameMetadata::END_OF_STREAM,
                                    &end_of_stream) &&
      end_of_stream) {
    rendering_frame_buffer_.reset();
    SetCurrentFrame(frame);
    return;
  }

  // If we detect a bad frame without |render_time|, we switch off algorithm,
  // because without |render_time|, algorithm cannot work.
  // In general, this should not happen.
  base::TimeTicks render_time;
  if (!frame->metadata()->GetTimeTicks(
          media::VideoFrameMetadata::REFERENCE_TIME, &render_time)) {
    DLOG(WARNING)
        << "Incoming VideoFrames have no REFERENCE_TIME, switching off super "
           "sophisticated rendering algorithm";
    rendering_frame_buffer_.reset();
    SetCurrentFrame(frame);
    return;
  }

  // The code below handles the case where UpdateCurrentFrame() callbacks stop.
  // These callbacks can stop when the tab is hidden or the page area containing
  // the video frame is scrolled out of view.
  // Since some hardware decoders only have a limited number of output frames,
  // we must aggressively release frames in this case.
  const base::TimeTicks now = base::TimeTicks::Now();
  if (now > last_deadline_max_) {
    // Note: the frame in |rendering_frame_buffer_| with lowest index is the
    // same as |current_frame_|. Function SetCurrentFrame() handles whether
    // to increase |dropped_frame_count_| for that frame, so here we should
    // increase |dropped_frame_count_| by the count of all other frames.
    dropped_frame_count_ += rendering_frame_buffer_->frames_queued() - 1;
    rendering_frame_buffer_->Reset();
    timestamps_to_clock_times_.clear();
    SetCurrentFrame(frame);
  }

  timestamps_to_clock_times_[frame->timestamp()] = render_time;
  rendering_frame_buffer_->EnqueueFrame(frame);
}

bool WebMediaPlayerMSCompositor::UpdateCurrentFrame(
    base::TimeTicks deadline_min,
    base::TimeTicks deadline_max) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());

  TRACE_EVENT_BEGIN2("webrtc", "WebMediaPlayerMS::UpdateCurrentFrame",
                     "Actual Render Begin", deadline_min.ToInternalValue(),
                     "Actual Render End", deadline_max.ToInternalValue());
  if (stopped_)
    return false;

  base::TimeTicks render_time;

  base::AutoLock auto_lock(current_frame_lock_);

  if (rendering_frame_buffer_)
    Render(deadline_min, deadline_max);

  if (!current_frame_->metadata()->GetTimeTicks(
          media::VideoFrameMetadata::REFERENCE_TIME, &render_time)) {
    DCHECK(!rendering_frame_buffer_)
        << "VideoFrames need REFERENCE_TIME to use "
           "sophisticated video rendering algorithm.";
  }

  TRACE_EVENT_END2("webrtc", "WebMediaPlayerMS::UpdateCurrentFrame",
                   "Ideal Render Instant", render_time.ToInternalValue(),
                   "Serial", serial_);

  return !current_frame_used_by_compositor_;
}

bool WebMediaPlayerMSCompositor::HasCurrentFrame() {
  base::AutoLock auto_lock(current_frame_lock_);
  return current_frame_.get() != nullptr;
}

scoped_refptr<media::VideoFrame> WebMediaPlayerMSCompositor::GetCurrentFrame() {
  DVLOG(3) << __func__;
  base::AutoLock auto_lock(current_frame_lock_);
  current_frame_used_by_compositor_ = true;
  return current_frame_;
}

void WebMediaPlayerMSCompositor::PutCurrentFrame() {
  DVLOG(3) << __func__;
}

scoped_refptr<media::VideoFrame>
WebMediaPlayerMSCompositor::GetCurrentFrameWithoutUpdatingStatistics() {
  DVLOG(3) << __func__;
  base::AutoLock auto_lock(current_frame_lock_);
  return current_frame_;
}

void WebMediaPlayerMSCompositor::StartRendering() {
  DCHECK(thread_checker_.CalledOnValidThread());
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WebMediaPlayerMSCompositor::StartRenderingInternal, this));
}

void WebMediaPlayerMSCompositor::StopRendering() {
  DCHECK(thread_checker_.CalledOnValidThread());
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WebMediaPlayerMSCompositor::StopRenderingInternal, this));
}

void WebMediaPlayerMSCompositor::ReplaceCurrentFrameWithACopy() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(current_frame_lock_);
  if (!current_frame_.get() || !player_)
    return;

  // Copy the frame so that rendering can show the last received frame.
  // The original frame must not be referenced when the player is paused since
  // there might be a finite number of available buffers. E.g, video that
  // originates from a video camera.
  current_frame_ =
      CopyFrame(current_frame_, player_->GetSkCanvasVideoRenderer());
}

void WebMediaPlayerMSCompositor::StopUsingProvider() {
  DCHECK(thread_checker_.CalledOnValidThread());
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WebMediaPlayerMSCompositor::StopUsingProviderInternal, this));
}

bool WebMediaPlayerMSCompositor::MapTimestampsToRenderTimeTicks(
    const std::vector<base::TimeDelta>& timestamps,
    std::vector<base::TimeTicks>* wall_clock_times) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread() ||
         thread_checker_.CalledOnValidThread() ||
         io_thread_checker_.CalledOnValidThread());
  for (const base::TimeDelta& timestamp : timestamps) {
    DCHECK(timestamps_to_clock_times_.count(timestamp));
    wall_clock_times->push_back(timestamps_to_clock_times_[timestamp]);
  }
  return true;
}

void WebMediaPlayerMSCompositor::Render(base::TimeTicks deadline_min,
                                        base::TimeTicks deadline_max) {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread() ||
         thread_checker_.CalledOnValidThread());
  current_frame_lock_.AssertAcquired();
  last_deadline_max_ = deadline_max;
  last_render_length_ = deadline_max - deadline_min;

  size_t frames_dropped = 0;
  scoped_refptr<media::VideoFrame> frame = rendering_frame_buffer_->Render(
      deadline_min, deadline_max, &frames_dropped);
  dropped_frame_count_ += frames_dropped;

  // There is a chance we get a null |frame| here:
  // When the player gets paused, we reset |rendering_frame_buffer_|;
  // When the player gets resumed, it is possible that render gets called before
  // we get a new frame. In that case continue to render the |current_frame_|.
  if (!frame || frame == current_frame_)
    return;

  SetCurrentFrame(frame);

  const auto& end = timestamps_to_clock_times_.end();
  const auto& begin = timestamps_to_clock_times_.begin();
  auto iterator = begin;
  while (iterator != end && iterator->first < frame->timestamp())
    ++iterator;
  timestamps_to_clock_times_.erase(begin, iterator);
}

void WebMediaPlayerMSCompositor::SetCurrentFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  current_frame_lock_.AssertAcquired();

  if (!current_frame_used_by_compositor_)
    ++dropped_frame_count_;
  current_frame_used_by_compositor_ = false;

  const bool size_changed =
      !current_frame_ ||
      current_frame_->natural_size() != frame->natural_size();
  current_frame_ = frame;
  if (size_changed) {
    main_message_loop_->task_runner()->PostTask(
        FROM_HERE, base::Bind(&WebMediaPlayerMS::TriggerResize, player_));
  }
  main_message_loop_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&WebMediaPlayerMS::ResetCanvasCache, player_));
}

void WebMediaPlayerMSCompositor::StartRenderingInternal() {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  stopped_ = false;

  if (video_frame_provider_client_)
    video_frame_provider_client_->StartRendering();
}

void WebMediaPlayerMSCompositor::StopRenderingInternal() {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  stopped_ = true;

  // It is possible that the video gets paused and then resumed. We need to
  // reset VideoRendererAlgorithm, otherwise, VideoRendererAlgorithm will think
  // there is a very long frame in the queue and then make totally wrong
  // frame selection.
  {
    base::AutoLock auto_lock(current_frame_lock_);
    if (rendering_frame_buffer_)
      rendering_frame_buffer_->Reset();
  }

  if (video_frame_provider_client_)
    video_frame_provider_client_->StopRendering();
}

void WebMediaPlayerMSCompositor::StopUsingProviderInternal() {
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
  if (video_frame_provider_client_)
    video_frame_provider_client_->StopUsingProvider();
  video_frame_provider_client_ = nullptr;
}

void WebMediaPlayerMSCompositor::SetAlgorithmEnabledForTesting(
    bool algorithm_enabled) {
  if (!algorithm_enabled) {
    rendering_frame_buffer_.reset();
    return;
  }

  if (!rendering_frame_buffer_) {
    rendering_frame_buffer_.reset(new media::VideoRendererAlgorithm(
        base::Bind(&WebMediaPlayerMSCompositor::MapTimestampsToRenderTimeTicks,
                   base::Unretained(this))));
  }
}

}  // namespace content
