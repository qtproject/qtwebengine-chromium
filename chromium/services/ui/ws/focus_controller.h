// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_FOCUS_CONTROLLER_H_
#define SERVICES_UI_WS_FOCUS_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "services/ui/ws/server_window_drawn_tracker_observer.h"
#include "services/ui/ws/server_window_tracker.h"

namespace ui {

namespace ws {

class FocusControllerDelegate;
class FocusControllerObserver;
class ServerWindow;
class ServerWindowDrawnTracker;

// Describes the source of the change.
enum class FocusControllerChangeSource {
  EXPLICIT,
  DRAWN_STATE_CHANGED,
};

// Tracks the focused window. Focus is moved to another window when the drawn
// state of the focused window changes.
class FocusController : public ServerWindowDrawnTrackerObserver {
 public:
  FocusController(FocusControllerDelegate* delegate, ServerWindow* root);
  ~FocusController() override;

  // Sets the focused window. Does nothing if |window| is currently focused.
  // This does not notify the delegate. See ServerWindow::SetFocusedWindow()
  // for details on return value.
  bool SetFocusedWindow(ServerWindow* window);
  ServerWindow* GetFocusedWindow();

  // Moves activation to the next activatable window.
  void ActivateNextWindow();

  void AddObserver(FocusControllerObserver* observer);
  void RemoveObserver(FocusControllerObserver* observer);

 private:
  enum class ActivationChangeReason {
    UNKNONW,
    CYCLE,  // Activation changed because of ActivateNextWindow().
    FOCUS,  // Focus change required a different window to be activated.
    DRAWN_STATE_CHANGED,  // Active window was hidden or destroyed.
  };
  void SetActiveWindow(ServerWindow* window, ActivationChangeReason reason);

  // Returns whether |window| can be focused or activated.
  bool CanBeFocused(ServerWindow* window) const;
  bool CanBeActivated(ServerWindow* window) const;

  // Returns the closest activatable ancestor of |window|. Returns nullptr if
  // there is no such ancestor.
  ServerWindow* GetActivatableAncestorOf(ServerWindow* window) const;

  // Implementation of SetFocusedWindow().
  bool SetFocusedWindowImpl(FocusControllerChangeSource change_source,
                            ServerWindow* window);

  // ServerWindowDrawnTrackerObserver:
  void OnDrawnStateWillChange(ServerWindow* ancestor,
                              ServerWindow* window,
                              bool is_drawn) override;
  void OnDrawnStateChanged(ServerWindow* ancestor,
                           ServerWindow* window,
                           bool is_drawn) override;

  FocusControllerDelegate* delegate_;

  ServerWindow* root_;
  ServerWindow* focused_window_;
  ServerWindow* active_window_;
  // Tracks what caused |active_window_| to be activated.
  ActivationChangeReason activation_reason_;

  // Keeps track of the list of windows that have already been visited during a
  // window cycle. This is only active when |activation_reason_| is set to
  // CYCLE.
  std::unique_ptr<ServerWindowTracker> cycle_windows_;

  base::ObserverList<FocusControllerObserver> observers_;

  // Keeps track of the visibility of the focused and active window.
  std::unique_ptr<ServerWindowDrawnTracker> drawn_tracker_;

  DISALLOW_COPY_AND_ASSIGN(FocusController);
};

}  // namespace ws

}  // namespace ui

#endif  // SERVICES_UI_WS_FOCUS_CONTROLLER_H_
