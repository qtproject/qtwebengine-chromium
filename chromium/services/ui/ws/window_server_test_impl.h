// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_WINDOW_SERVER_TEST_IMPL_H_
#define SERVICES_UI_WS_WINDOW_SERVER_TEST_IMPL_H_

#include "services/ui/public/interfaces/window_server_test.mojom.h"

namespace ui {
namespace ws {

class ServerWindow;
class WindowServer;

class WindowServerTestImpl : public mojom::WindowServerTest {
 public:
  explicit WindowServerTestImpl(WindowServer* server);
  ~WindowServerTestImpl() override;

 private:
  void OnWindowPaint(const std::string& name,
                     const EnsureClientHasDrawnWindowCallback& cb,
                     ServerWindow* window);

  // mojom::WindowServerTest:
  void EnsureClientHasDrawnWindow(
      const std::string& client_name,
      const EnsureClientHasDrawnWindowCallback& callback) override;

  WindowServer* window_server_;

  DISALLOW_COPY_AND_ASSIGN(WindowServerTestImpl);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_WINDOW_SERVER_TEST_IMPL_H_
