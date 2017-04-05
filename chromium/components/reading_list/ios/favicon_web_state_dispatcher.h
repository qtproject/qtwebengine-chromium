// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_IOS_FAVICON_WEB_STATE_DISPATCHER_H_
#define COMPONENTS_READING_LIST_IOS_FAVICON_WEB_STATE_DISPATCHER_H_

namespace web {
class WebState;
}

namespace reading_list {

// Dispatcher for WebState having a Favicon Driver, with BookmarkModel and
// HistoryService attached, as observer. After a WebState is returned, the
// dispatcher keeps it alive long enough for it to download the favicons.
class FaviconWebStateDispatcher {
 public:
  FaviconWebStateDispatcher() {}
  virtual ~FaviconWebStateDispatcher() {}
  // Returns a WebState with a Favicon Driver attached.
  virtual std::unique_ptr<web::WebState> RequestWebState() = 0;
  // Called to return a WebState. The WebState should not be used after being
  // returned.
  virtual void ReturnWebState(std::unique_ptr<web::WebState> web_state) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(FaviconWebStateDispatcher);
};

}  // namespace reading_list

#endif  // COMPONENTS_READING_LIST_IOS_FAVICON_WEB_STATE_DISPATCHER_H_
