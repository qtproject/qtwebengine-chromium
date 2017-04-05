/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/webmidi/MIDIPort.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/DOMException.h"
#include "modules/webmidi/MIDIAccess.h"
#include "modules/webmidi/MIDIConnectionEvent.h"

using midi::mojom::PortState;

namespace blink {

MIDIPort::MIDIPort(MIDIAccess* access,
                   const String& id,
                   const String& manufacturer,
                   const String& name,
                   TypeCode type,
                   const String& version,
                   PortState state)
    : ContextLifecycleObserver(access->getExecutionContext()),
      m_id(id),
      m_manufacturer(manufacturer),
      m_name(name),
      m_type(type),
      m_version(version),
      m_access(this, access),
      m_connection(ConnectionStateClosed) {
  DCHECK(access);
  DCHECK(type == TypeInput || type == TypeOutput);
  DCHECK(state == PortState::DISCONNECTED || state == PortState::CONNECTED);
  m_state = state;
}

String MIDIPort::connection() const {
  switch (m_connection) {
    case ConnectionStateOpen:
      return "open";
    case ConnectionStateClosed:
      return "closed";
    case ConnectionStatePending:
      return "pending";
  }
  return emptyString();
}

String MIDIPort::state() const {
  switch (m_state) {
    case PortState::DISCONNECTED:
      return "disconnected";
    case PortState::CONNECTED:
      return "connected";
    case PortState::OPENED:
      NOTREACHED();
      return "connected";
  }
  return emptyString();
}

String MIDIPort::type() const {
  switch (m_type) {
    case TypeInput:
      return "input";
    case TypeOutput:
      return "output";
  }
  return emptyString();
}

ScriptPromise MIDIPort::open(ScriptState* scriptState) {
  open();
  return accept(scriptState);
}

ScriptPromise MIDIPort::close(ScriptState* scriptState) {
  if (m_connection != ConnectionStateClosed) {
    // TODO(toyoshim): Do clear() operation on MIDIOutput.
    // TODO(toyoshim): Add blink API to perform a real close operation.
    setStates(m_state, ConnectionStateClosed);
  }
  return accept(scriptState);
}

void MIDIPort::setState(PortState state) {
  switch (state) {
    case PortState::DISCONNECTED:
      switch (m_connection) {
        case ConnectionStateOpen:
        case ConnectionStatePending:
          setStates(PortState::DISCONNECTED, ConnectionStatePending);
          break;
        case ConnectionStateClosed:
          // Will do nothing.
          setStates(PortState::DISCONNECTED, ConnectionStateClosed);
          break;
      }
      break;
    case PortState::CONNECTED:
      switch (m_connection) {
        case ConnectionStateOpen:
          NOTREACHED();
          break;
        case ConnectionStatePending:
          // We do not use |setStates| in order not to dispatch events twice.
          // |open| calls |setStates|.
          m_state = PortState::CONNECTED;
          open();
          break;
        case ConnectionStateClosed:
          setStates(PortState::CONNECTED, ConnectionStateClosed);
          break;
      }
      break;
    case PortState::OPENED:
      NOTREACHED();
      break;
  }
}

ExecutionContext* MIDIPort::getExecutionContext() const {
  return m_access->getExecutionContext();
}

bool MIDIPort::hasPendingActivity() const {
  // MIDIPort should survive if ConnectionState is "open" or can be "open" via
  // a MIDIConnectionEvent even if there are no references from JavaScript.
  return m_connection != ConnectionStateClosed;
}

void MIDIPort::contextDestroyed(ExecutionContext*) {
  // Should be "closed" to assume there are no pending activities.
  m_connection = ConnectionStateClosed;
}

DEFINE_TRACE(MIDIPort) {
  visitor->trace(m_access);
  EventTargetWithInlineData::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
}

DEFINE_TRACE_WRAPPERS(MIDIPort) {
  visitor->traceWrappers(m_access);
  EventTargetWithInlineData::traceWrappers(visitor);
}

void MIDIPort::open() {
  switch (m_state) {
    case PortState::DISCONNECTED:
      setStates(m_state, ConnectionStatePending);
      break;
    case PortState::CONNECTED:
      // TODO(toyoshim): Add blink API to perform a real open and close
      // operation.
      setStates(m_state, ConnectionStateOpen);
      break;
    case PortState::OPENED:
      NOTREACHED();
      break;
  }
}

ScriptPromise MIDIPort::accept(ScriptState* scriptState) {
  return ScriptPromise::cast(
      scriptState,
      ToV8(this, scriptState->context()->Global(), scriptState->isolate()));
}

ScriptPromise MIDIPort::reject(ScriptState* scriptState,
                               ExceptionCode ec,
                               const String& message) {
  return ScriptPromise::rejectWithDOMException(
      scriptState, DOMException::create(ec, message));
}

void MIDIPort::setStates(PortState state, ConnectionState connection) {
  DCHECK(state != PortState::DISCONNECTED || connection != ConnectionStateOpen);
  if (m_state == state && m_connection == connection)
    return;
  m_state = state;
  m_connection = connection;
  dispatchEvent(MIDIConnectionEvent::create(this));
  m_access->dispatchEvent(MIDIConnectionEvent::create(this));
}

}  // namespace blink
