// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_monitor.h"

#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "device/gamepad/gamepad_service.h"
#include "device/gamepad/gamepad_shared_buffer.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace device {

GamepadMonitor::GamepadMonitor() : is_started_(false) {}

GamepadMonitor::~GamepadMonitor() {
  if (is_started_)
    GamepadService::GetInstance()->RemoveConsumer(this);
}

// static
void GamepadMonitor::Create(mojom::GamepadMonitorRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<GamepadMonitor>(),
                          std::move(request));
}

void GamepadMonitor::OnGamepadConnected(unsigned index,
                                        const blink::WebGamepad& gamepad) {
  if (gamepad_observer_)
    gamepad_observer_->GamepadConnected(index, gamepad);
}

void GamepadMonitor::OnGamepadDisconnected(unsigned index,
                                           const blink::WebGamepad& gamepad) {
  if (gamepad_observer_)
    gamepad_observer_->GamepadDisconnected(index, gamepad);
}

void GamepadMonitor::GamepadStartPolling(
    const GamepadStartPollingCallback& callback) {
  DCHECK(!is_started_);
  is_started_ = true;

  GamepadService* service = GamepadService::GetInstance();
  service->ConsumerBecameActive(this);
  callback.Run(service->GetSharedBufferHandle());
}

void GamepadMonitor::GamepadStopPolling(
    const GamepadStopPollingCallback& callback) {
  DCHECK(is_started_);
  is_started_ = false;

  GamepadService::GetInstance()->ConsumerBecameInactive(this);
  callback.Run();
}

void GamepadMonitor::SetObserver(mojom::GamepadObserverPtr gamepad_observer) {
  gamepad_observer_ = std::move(gamepad_observer);
}

}  // namespace device
