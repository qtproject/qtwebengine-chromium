// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_BASE_RESOURCE_THROTTLE_H_
#define COMPONENTS_SAFE_BROWSING_BASE_RESOURCE_THROTTLE_H_

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/safe_browsing/base_ui_manager.h"
#include "components/safe_browsing_db/database_manager.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/common/resource_type.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "url/gurl.h"

namespace content {
class ResourceRequestInfo;
}

namespace net {
class URLRequest;
}

namespace safe_browsing {

// BaseResourceThrottle checks that URLs are "safe" before
// navigating to them. To be considered "safe", a URL must not appear in the
// malware/phishing blacklists (see SafeBrowsingService for details).
//
// Note that the safe browsing check takes at most kCheckUrlTimeoutMs
// milliseconds. If it takes longer than this, then the system defaults to
// treating the URL as safe.
//
// If the URL is classified as dangerous, a warning page is thrown up and
// the request remains suspended.  If the user clicks "proceed" on warning
// page, we resume the request.
//
// Note: The ResourceThrottle interface is called in this order:
// WillStartRequest once, WillRedirectRequest zero or more times, and then
// WillProcessReponse once.
class BaseResourceThrottle
    : public content::ResourceThrottle,
      public SafeBrowsingDatabaseManager::Client,
      public base::SupportsWeakPtr<BaseResourceThrottle> {
 public:
  // Construct a BaseResourceThrottle, or return nullptr if we
  // cannot access the safe browsing API on Android
  static BaseResourceThrottle* MaybeCreate(
      net::URLRequest* request,
      content::ResourceType resource_type,
      scoped_refptr<SafeBrowsingDatabaseManager>
          database_manager,
      scoped_refptr<BaseUIManager> ui_manager);

  // content::ResourceThrottle implementation (called on IO thread):
  void WillStartRequest(bool* defer) override;
  void WillRedirectRequest(const net::RedirectInfo& redirect_info,
                           bool* defer) override;
  void WillProcessResponse(bool* defer) override;
  bool MustProcessResponseBeforeReadingBody() override;

  const char* GetNameForLogging() const override;

  // SafeBrowsingDatabaseManager::Client implementation (called on IO thread):
  void OnCheckBrowseUrlResult(
      const GURL& url,
      SBThreatType threat_type,
      const ThreatMetadata& metadata) override;

 protected:
  BaseResourceThrottle(
      const net::URLRequest* request,
      content::ResourceType resource_type,
      scoped_refptr<SafeBrowsingDatabaseManager>
          database_manager,
      scoped_refptr<BaseUIManager> ui_manager);

  ~BaseResourceThrottle() override;

  // Does nothing in the base class. Override this to destroy prerender contents
  // in chrome.
  virtual void MaybeDestroyPrerenderContents(
      const content::ResourceRequestInfo* info);

  // Posts a task for StartDisplayingBlockingPage
  virtual void StartDisplayingBlockingPageHelper(
      security_interstitials::UnsafeResource resource);

  // Called by OnBlockingPageComplete when proceed == false. This removes the
  // blocking page. This calls ResourceThrottle::Cancel() to show the previous
  // page, but may be overridden in a subclass.
  virtual void CancelResourceLoad();

  scoped_refptr<BaseUIManager> ui_manager();

 private:
  // Describes what phase of the check a throttle is in.
  enum State {
    // Haven't started checking or checking is complete. Not deferred.
    STATE_NONE,
    // We have one outstanding URL-check. Could be deferred.
    STATE_CHECKING_URL,
    // We're displaying a blocking page. Could be deferred.
    STATE_DISPLAYING_BLOCKING_PAGE,
  };

  // Describes what stage of the request got paused by the check.
  enum DeferState {
    DEFERRED_NONE,
    DEFERRED_START,
    DEFERRED_REDIRECT,
    DEFERRED_UNCHECKED_REDIRECT,  // unchecked_redirect_url_ is populated.
    DEFERRED_PROCESSING,
  };

  scoped_refptr<BaseUIManager> ui_manager_;

  // Called on the IO thread when the user has decided to proceed with the
  // current request, or go back.
  void OnBlockingPageComplete(bool proceed);

  // Starts running |url| through the safe browsing check. Returns true if the
  // URL is safe to visit. Otherwise returns false and will call
  // OnBrowseUrlResult() when the check has completed.
  bool CheckUrl(const GURL& url);

  // Callback for when the safe browsing check (which was initiated by
  // StartCheckingUrl()) has taken longer than kCheckUrlTimeoutMs.
  void OnCheckUrlTimeout();

  // Starts displaying the safe browsing interstitial page. Called on the UI
  // thread.
  static void StartDisplayingBlockingPage(
      const base::WeakPtr<BaseResourceThrottle>& throttle,
      scoped_refptr<BaseUIManager> ui_manager,
      const security_interstitials::UnsafeResource& resource);

  void ResumeRequest();

  // For marking network events.  |name| and |value| can be null.
  void BeginNetLogEvent(net::NetLogEventType type,
                        const GURL& url,
                        const char* name,
                        const char* value);
  void EndNetLogEvent(net::NetLogEventType type,
                      const char* name,
                      const char* value);

  // The result of the most recent safe browsing check. Only valid to read this
  // when state_ != STATE_CHECKING_URL.
  safe_browsing::SBThreatType threat_type_;

  // The time when we started deferring the request.
  base::TimeTicks defer_start_time_;

  // Timer to abort the safe browsing check if it takes too long.
  base::OneShotTimer timer_;

  // The redirect chain for this resource
  std::vector<GURL> redirect_urls_;

  // If in DEFERRED_UNCHECKED_REDIRECT state, this is the
  // URL we still need to check before resuming.
  GURL unchecked_redirect_url_;
  GURL url_being_checked_;

  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  const net::URLRequest* request_;

  State state_;
  DeferState defer_state_;

  const content::ResourceType resource_type_;
  net::NetLogWithSource net_log_with_source_;

  // TODO(vakh): The following set should be removed after fixing
  // http://crbug.com/660293
  // URLs that timed out waiting for a SafeBrowsing reputation check.
  std::set<GURL> timed_out_urls_;

  DISALLOW_COPY_AND_ASSIGN(BaseResourceThrottle);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_BASE_RESOURCE_THROTTLE_H_
