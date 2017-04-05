// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_H_

#include <string>

#include "base/macros.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace service_manager {

class InterfaceRegistry;
class ServiceContext;
struct ServiceInfo;

// The primary contract between a Service and the Service Manager, receiving
// lifecycle notifications and connection requests.
class Service {
 public:
  Service();
  virtual ~Service();

  // Called exactly once, when a bidirectional connection with the Service
  // Manager has been established. No calls to OnConnect() will be received
  // before this.
  virtual void OnStart();

  // Called each time a connection to this service is brokered by the Service
  // Manager. Implement this to expose interfaces to other services.
  //
  // Return true if the connection should succeed or false if the connection
  // should be rejected.
  //
  // The default implementation returns false.
  virtual bool OnConnect(const ServiceInfo& remote_info,
                         InterfaceRegistry* registry);

  // Called when the service identified by |source_info| requests this service
  // bind a request for |interface_name|. If this method has been called, the
  // service manager has already determined that policy permits this interface
  // to be bound, so the implementation of this method can trust that it should
  // just blindly bind it under most conditions.
  virtual void OnBindInterface(const ServiceInfo& source_info,
                               const std::string& interface_name,
                               mojo::ScopedMessagePipeHandle interface_pipe);

  // Called when the Service Manager has stopped tracking this instance. The
  // service should use this as a signal to shut down, and in fact its process
  // may be reaped shortly afterward if applicable.
  //
  // Return true from this method to tell the ServiceContext to signal its
  // shutdown extenrally (i.e. to invoke it's "connection lost" closure if set),
  // or return false to defer the signal. If deferred, the Service should
  // explicitly call QuitNow() on the ServiceContext when it's ready to be
  // torn down.
  //
  // The default implementation returns true.
  //
  // While it's possible for this to be invoked before either OnStart() or
  // OnConnect() is invoked, neither will be invoked at any point after this
  // OnStop().
  virtual bool OnStop();

 protected:
  // Access the ServiceContext associated with this Service. Note that this is
  // only valid to call during or after OnStart(), but never before! As such,
  // it's always safe to call in OnStart() and OnConnect(), but should generally
  // be avoided in OnStop().
  ServiceContext* context() const;

 private:
  friend class ForwardingService;
  friend class ServiceContext;

  // NOTE: This is guaranteed to be called before OnStart().
  void set_context(ServiceContext* context) { service_context_ = context; }

  ServiceContext* service_context_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

// TODO(rockot): Remove this. It's here to satisfy a few remaining use cases
// where a Service impl is owned by something other than its ServiceContext.
class ForwardingService : public Service {
 public:
  // |target| must outlive this object.
  explicit ForwardingService(Service* target);
  ~ForwardingService() override;

  // Service:
  void OnStart() override;
  bool OnConnect(const ServiceInfo& remote_info,
                 InterfaceRegistry* registry) override;
  bool OnStop() override;

 private:
  Service* const target_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ForwardingService);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_H_
