// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/networking_private/networking_private_chromeos.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_activation_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_util.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translator.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "chromeos/network/proxy/ui_proxy_config.h"
#include "chromeos/network/proxy/ui_proxy_config_service.h"
#include "components/onc/onc_constants.h"
#include "components/proxy_config/proxy_prefs.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/networking_private/networking_private_api.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using chromeos::NetworkHandler;
using chromeos::NetworkStateHandler;
using chromeos::NetworkTypePattern;
using chromeos::ShillManagerClient;
using chromeos::UIProxyConfig;
using extensions::NetworkingPrivateDelegate;

namespace private_api = extensions::api::networking_private;

namespace {

chromeos::NetworkStateHandler* GetStateHandler() {
  return NetworkHandler::Get()->network_state_handler();
}

chromeos::ManagedNetworkConfigurationHandler* GetManagedConfigurationHandler() {
  return NetworkHandler::Get()->managed_network_configuration_handler();
}

bool GetServicePathFromGuid(const std::string& guid,
                            std::string* service_path,
                            std::string* error) {
  const chromeos::NetworkState* network =
      GetStateHandler()->GetNetworkStateFromGuid(guid);
  if (!network) {
    *error = extensions::networking_private::kErrorInvalidNetworkGuid;
    return false;
  }
  *service_path = network->path();
  return true;
}

bool GetPrimaryUserIdHash(content::BrowserContext* browser_context,
                          std::string* user_hash,
                          std::string* error) {
  std::string context_user_hash =
      extensions::ExtensionsBrowserClient::Get()->GetUserIdHashFromContext(
          browser_context);

  // Currently Chrome OS only configures networks for the primary user.
  // Configuration attempts from other browser contexts should fail.
  if (context_user_hash != chromeos::LoginState::Get()->primary_user_hash()) {
    // Disallow class requiring a user id hash from a non-primary user context
    // to avoid complexities with the policy code.
    LOG(ERROR) << "networkingPrivate API call from non primary user: "
               << context_user_hash;
    if (error)
      *error = "Error.NonPrimaryUser";
    return false;
  }
  if (user_hash)
    *user_hash = context_user_hash;
  return true;
}

void AppendDeviceState(
    const std::string& type,
    const chromeos::DeviceState* device,
    NetworkingPrivateDelegate::DeviceStateList* device_state_list) {
  DCHECK(!type.empty());
  NetworkTypePattern pattern =
      chromeos::onc::NetworkTypePatternFromOncType(type);
  NetworkStateHandler::TechnologyState technology_state =
      GetStateHandler()->GetTechnologyState(pattern);
  private_api::DeviceStateType state = private_api::DEVICE_STATE_TYPE_NONE;
  switch (technology_state) {
    case NetworkStateHandler::TECHNOLOGY_UNAVAILABLE:
      if (!device)
        return;
      // If we have a DeviceState entry but the technology is not available,
      // assume the technology is not initialized.
      state = private_api::DEVICE_STATE_TYPE_UNINITIALIZED;
      break;
    case NetworkStateHandler::TECHNOLOGY_AVAILABLE:
      state = private_api::DEVICE_STATE_TYPE_DISABLED;
      break;
    case NetworkStateHandler::TECHNOLOGY_UNINITIALIZED:
      state = private_api::DEVICE_STATE_TYPE_UNINITIALIZED;
      break;
    case NetworkStateHandler::TECHNOLOGY_ENABLING:
      state = private_api::DEVICE_STATE_TYPE_ENABLING;
      break;
    case NetworkStateHandler::TECHNOLOGY_ENABLED:
      state = private_api::DEVICE_STATE_TYPE_ENABLED;
      break;
    case NetworkStateHandler::TECHNOLOGY_PROHIBITED:
      state = private_api::DEVICE_STATE_TYPE_PROHIBITED;
      break;
  }
  DCHECK_NE(private_api::DEVICE_STATE_TYPE_NONE, state);
  std::unique_ptr<private_api::DeviceStateProperties> properties(
      new private_api::DeviceStateProperties);
  properties->type = private_api::ParseNetworkType(type);
  properties->state = state;
  if (device && state == private_api::DEVICE_STATE_TYPE_ENABLED)
    properties->scanning.reset(new bool(device->scanning()));
  if (device && type == ::onc::network_config::kCellular) {
    properties->sim_present.reset(new bool(!device->IsSimAbsent()));
    if (!device->sim_lock_type().empty())
      properties->sim_lock_type.reset(new std::string(device->sim_lock_type()));
  }
  device_state_list->push_back(std::move(properties));
}

void NetworkHandlerFailureCallback(
    const NetworkingPrivateDelegate::FailureCallback& callback,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  callback.Run(error_name);
}

void RequirePinSuccess(
    const std::string& device_path,
    const std::string& current_pin,
    const std::string& new_pin,
    const extensions::NetworkingPrivateChromeOS::VoidCallback& success_callback,
    const extensions::NetworkingPrivateChromeOS::FailureCallback&
        failure_callback) {
  // After RequirePin succeeds, call ChangePIN iff a different new_pin is
  // provided.
  if (new_pin.empty() || new_pin == current_pin) {
    success_callback.Run();
    return;
  }
  NetworkHandler::Get()->network_device_handler()->ChangePin(
      device_path, current_pin, new_pin, success_callback,
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

// Returns the string corresponding to |key|. If the property is a managed
// dictionary, returns the active value. If the property does not exist or
// has no active value, returns an empty string.
std::string GetStringFromDictionary(const base::DictionaryValue& dictionary,
                                    const std::string& key) {
  std::string result;
  if (!dictionary.GetStringWithoutPathExpansion(key, &result)) {
    const base::DictionaryValue* managed = nullptr;
    if (dictionary.GetDictionaryWithoutPathExpansion(key, &managed)) {
      managed->GetStringWithoutPathExpansion(::onc::kAugmentationActiveSetting,
                                             &result);
    }
  }
  return result;
}

base::DictionaryValue* GetThirdPartyVPNDictionary(
    base::DictionaryValue* dictionary) {
  const std::string type =
      GetStringFromDictionary(*dictionary, ::onc::network_config::kType);
  if (type != ::onc::network_config::kVPN)
    return nullptr;
  base::DictionaryValue* vpn_dict = nullptr;
  if (!dictionary->GetDictionary(::onc::network_config::kVPN, &vpn_dict))
    return nullptr;
  if (GetStringFromDictionary(*vpn_dict, ::onc::vpn::kType) !=
      ::onc::vpn::kThirdPartyVpn) {
    return nullptr;
  }
  base::DictionaryValue* third_party_vpn = nullptr;
  vpn_dict->GetDictionary(::onc::vpn::kThirdPartyVpn, &third_party_vpn);
  return third_party_vpn;
}

const chromeos::DeviceState* GetCellularDeviceState(const std::string& guid) {
  const chromeos::NetworkState* network_state = nullptr;
  if (!guid.empty())
    network_state = GetStateHandler()->GetNetworkStateFromGuid(guid);
  const chromeos::DeviceState* device_state = nullptr;
  if (network_state) {
    device_state =
        GetStateHandler()->GetDeviceState(network_state->device_path());
  }
  if (!device_state) {
    device_state =
        GetStateHandler()->GetDeviceStateByType(NetworkTypePattern::Cellular());
  }
  return device_state;
}

// Ensures that |container| has a DictionaryValue for |key| and returns it.
base::DictionaryValue* EnsureDictionaryValue(const std::string& key,
                                             base::DictionaryValue* container) {
  base::DictionaryValue* dict;
  if (!container->GetDictionary(key, &dict)) {
    container->SetWithoutPathExpansion(
        key, base::MakeUnique<base::DictionaryValue>());
    container->GetDictionary(key, &dict);
  }
  return dict;
}

// Sets the active and effective ONC dictionary values of |dict| based on
// |state|. Also sets the UserEditable property to false.
void SetProxyEffectiveValue(base::DictionaryValue* dict,
                            ProxyPrefs::ConfigState state,
                            std::unique_ptr<base::Value> value) {
  // NOTE: ProxyPrefs::ConfigState only exposes 'CONFIG_POLICY' so just use
  // 'UserPolicy' here for the effective type. The existing UI does not
  // differentiate between policy types. TODO(stevenjb): Eliminate UIProxyConfig
  // and instead generate a proper ONC dictionary with the correct policy source
  // preserved. crbug.com/662529.
  dict->SetStringWithoutPathExpansion(::onc::kAugmentationEffectiveSetting,
                                      state == ProxyPrefs::CONFIG_EXTENSION
                                          ? ::onc::kAugmentationActiveExtension
                                          : ::onc::kAugmentationUserPolicy);
  if (state != ProxyPrefs::CONFIG_EXTENSION) {
    std::unique_ptr<base::Value> value_copy(value->CreateDeepCopy());
    dict->SetWithoutPathExpansion(::onc::kAugmentationUserPolicy,
                                  std::move(value_copy));
  }
  dict->SetWithoutPathExpansion(::onc::kAugmentationActiveSetting,
                                std::move(value));
  dict->SetBooleanWithoutPathExpansion(::onc::kAugmentationUserEditable, false);
}

std::string GetProxySettingsType(const UIProxyConfig::Mode& mode) {
  switch (mode) {
    case UIProxyConfig::MODE_DIRECT:
      return ::onc::proxy::kDirect;
    case UIProxyConfig::MODE_AUTO_DETECT:
      return ::onc::proxy::kWPAD;
    case UIProxyConfig::MODE_PAC_SCRIPT:
      return ::onc::proxy::kPAC;
    case UIProxyConfig::MODE_SINGLE_PROXY:
    case UIProxyConfig::MODE_PROXY_PER_SCHEME:
      return ::onc::proxy::kManual;
  }
  NOTREACHED();
  return ::onc::proxy::kDirect;
}

void SetManualProxy(base::DictionaryValue* manual,
                    ProxyPrefs::ConfigState state,
                    const std::string& key,
                    const UIProxyConfig::ManualProxy& proxy) {
  base::DictionaryValue* dict = EnsureDictionaryValue(key, manual);
  base::DictionaryValue* host_dict =
      EnsureDictionaryValue(::onc::proxy::kHost, dict);
  SetProxyEffectiveValue(host_dict, state,
                         base::MakeUnique<base::StringValue>(
                             proxy.server.host_port_pair().host()));
  uint16_t port = proxy.server.host_port_pair().port();
  base::DictionaryValue* port_dict =
      EnsureDictionaryValue(::onc::proxy::kPort, dict);
  SetProxyEffectiveValue(port_dict, state,
                         base::MakeUnique<base::FundamentalValue>(port));
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////

namespace extensions {

NetworkingPrivateChromeOS::NetworkingPrivateChromeOS(
    content::BrowserContext* browser_context,
    std::unique_ptr<VerifyDelegate> verify_delegate)
    : NetworkingPrivateDelegate(std::move(verify_delegate)),
      browser_context_(browser_context),
      weak_ptr_factory_(this) {}

NetworkingPrivateChromeOS::~NetworkingPrivateChromeOS() {}

void NetworkingPrivateChromeOS::GetProperties(
    const std::string& guid,
    const DictionaryCallback& success_callback,
    const FailureCallback& failure_callback) {
  std::string service_path, error;
  if (!GetServicePathFromGuid(guid, &service_path, &error)) {
    failure_callback.Run(error);
    return;
  }

  std::string user_id_hash;
  if (!GetPrimaryUserIdHash(browser_context_, &user_id_hash, &error)) {
    failure_callback.Run(error);
    return;
  }

  GetManagedConfigurationHandler()->GetProperties(
      user_id_hash, service_path,
      base::Bind(&NetworkingPrivateChromeOS::GetPropertiesCallback,
                 weak_ptr_factory_.GetWeakPtr(), guid, false /* managed */,
                 success_callback),
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

void NetworkingPrivateChromeOS::GetManagedProperties(
    const std::string& guid,
    const DictionaryCallback& success_callback,
    const FailureCallback& failure_callback) {
  std::string service_path, error;
  if (!GetServicePathFromGuid(guid, &service_path, &error)) {
    failure_callback.Run(error);
    return;
  }

  std::string user_id_hash;
  if (!GetPrimaryUserIdHash(browser_context_, &user_id_hash, &error)) {
    failure_callback.Run(error);
    return;
  }

  GetManagedConfigurationHandler()->GetManagedProperties(
      user_id_hash, service_path,
      base::Bind(&NetworkingPrivateChromeOS::GetPropertiesCallback,
                 weak_ptr_factory_.GetWeakPtr(), guid, true /* managed */,
                 success_callback),
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

void NetworkingPrivateChromeOS::GetState(
    const std::string& guid,
    const DictionaryCallback& success_callback,
    const FailureCallback& failure_callback) {
  std::string service_path, error;
  if (!GetServicePathFromGuid(guid, &service_path, &error)) {
    failure_callback.Run(error);
    return;
  }

  const chromeos::NetworkState* network_state =
      GetStateHandler()->GetNetworkStateFromServicePath(
          service_path, false /* configured_only */);
  if (!network_state) {
    failure_callback.Run(networking_private::kErrorNetworkUnavailable);
    return;
  }

  std::unique_ptr<base::DictionaryValue> network_properties =
      chromeos::network_util::TranslateNetworkStateToONC(network_state);
  AppendThirdPartyProviderName(network_properties.get());

  success_callback.Run(std::move(network_properties));
}

void NetworkingPrivateChromeOS::SetProperties(
    const std::string& guid,
    std::unique_ptr<base::DictionaryValue> properties,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  std::string service_path, error;
  if (!GetServicePathFromGuid(guid, &service_path, &error)) {
    failure_callback.Run(error);
    return;
  }

  GetManagedConfigurationHandler()->SetProperties(
      service_path, *properties, success_callback,
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

void NetworkHandlerCreateCallback(
    const NetworkingPrivateDelegate::StringCallback& callback,
    const std::string& service_path,
    const std::string& guid) {
  callback.Run(guid);
}

void NetworkingPrivateChromeOS::CreateNetwork(
    bool shared,
    std::unique_ptr<base::DictionaryValue> properties,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  std::string user_id_hash, error;
  // Do not allow configuring a non-shared network from a non-primary user.
  if (!shared &&
      !GetPrimaryUserIdHash(browser_context_, &user_id_hash, &error)) {
    failure_callback.Run(error);
    return;
  }

  GetManagedConfigurationHandler()->CreateConfiguration(
      user_id_hash, *properties,
      base::Bind(&NetworkHandlerCreateCallback, success_callback),
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

void NetworkingPrivateChromeOS::ForgetNetwork(
    const std::string& guid,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  std::string service_path, error;
  if (!GetServicePathFromGuid(guid, &service_path, &error)) {
    failure_callback.Run(error);
    return;
  }

  GetManagedConfigurationHandler()->RemoveConfiguration(
      service_path, success_callback,
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

void NetworkingPrivateChromeOS::GetNetworks(
    const std::string& network_type,
    bool configured_only,
    bool visible_only,
    int limit,
    const NetworkListCallback& success_callback,
    const FailureCallback& failure_callback) {
  NetworkTypePattern pattern =
      chromeos::onc::NetworkTypePatternFromOncType(network_type);
  std::unique_ptr<base::ListValue> network_properties_list =
      chromeos::network_util::TranslateNetworkListToONC(
          pattern, configured_only, visible_only, limit);

  for (const auto& value : *network_properties_list) {
    base::DictionaryValue* network_dict = nullptr;
    value->GetAsDictionary(&network_dict);
    DCHECK(network_dict);
    if (GetThirdPartyVPNDictionary(network_dict))
      AppendThirdPartyProviderName(network_dict);
  }

  success_callback.Run(std::move(network_properties_list));
}

void NetworkingPrivateChromeOS::StartConnect(
    const std::string& guid,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  std::string service_path, error;
  if (!GetServicePathFromGuid(guid, &service_path, &error)) {
    failure_callback.Run(error);
    return;
  }

  const bool check_error_state = false;
  NetworkHandler::Get()->network_connection_handler()->ConnectToNetwork(
      service_path, success_callback,
      base::Bind(&NetworkingPrivateChromeOS::ConnectFailureCallback,
                 weak_ptr_factory_.GetWeakPtr(), guid, success_callback,
                 failure_callback),
      check_error_state);
}

void NetworkingPrivateChromeOS::StartDisconnect(
    const std::string& guid,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  std::string service_path, error;
  if (!GetServicePathFromGuid(guid, &service_path, &error)) {
    failure_callback.Run(error);
    return;
  }

  NetworkHandler::Get()->network_connection_handler()->DisconnectNetwork(
      service_path, success_callback,
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

void NetworkingPrivateChromeOS::StartActivate(
    const std::string& guid,
    const std::string& specified_carrier,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  const chromeos::NetworkState* network =
      GetStateHandler()->GetNetworkStateFromGuid(guid);
  if (!network) {
    failure_callback.Run(
        extensions::networking_private::kErrorInvalidNetworkGuid);
    return;
  }

  std::string carrier(specified_carrier);
  if (carrier.empty()) {
    const chromeos::DeviceState* device =
        GetStateHandler()->GetDeviceState(network->device_path());
    if (device)
      carrier = device->carrier();
  }
  if (carrier != shill::kCarrierSprint) {
    // Only Sprint is directly activated. For other carriers, show the
    // account details page.
    if (ui_delegate())
      ui_delegate()->ShowAccountDetails(guid);
    success_callback.Run();
    return;
  }

  if (!network->RequiresActivation()) {
    // If no activation is required, show the account details page.
    if (ui_delegate())
      ui_delegate()->ShowAccountDetails(guid);
    success_callback.Run();
    return;
  }

  NetworkHandler::Get()->network_activation_handler()->Activate(
      network->path(), carrier, success_callback,
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

void NetworkingPrivateChromeOS::SetWifiTDLSEnabledState(
    const std::string& ip_or_mac_address,
    bool enabled,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  NetworkHandler::Get()->network_device_handler()->SetWifiTDLSEnabled(
      ip_or_mac_address, enabled, success_callback,
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

void NetworkingPrivateChromeOS::GetWifiTDLSStatus(
    const std::string& ip_or_mac_address,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  NetworkHandler::Get()->network_device_handler()->GetWifiTDLSStatus(
      ip_or_mac_address, success_callback,
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

void NetworkingPrivateChromeOS::GetCaptivePortalStatus(
    const std::string& guid,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  if (!chromeos::network_portal_detector::IsInitialized()) {
    failure_callback.Run(networking_private::kErrorNotReady);
    return;
  }

  success_callback.Run(
      chromeos::NetworkPortalDetector::CaptivePortalStatusString(
          chromeos::network_portal_detector::GetInstance()
              ->GetCaptivePortalState(guid)
              .status));
}

void NetworkingPrivateChromeOS::UnlockCellularSim(
    const std::string& guid,
    const std::string& pin,
    const std::string& puk,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  const chromeos::DeviceState* device_state = GetCellularDeviceState(guid);
  if (!device_state) {
    failure_callback.Run(networking_private::kErrorNetworkUnavailable);
    return;
  }
  std::string lock_type = device_state->sim_lock_type();
  if (lock_type.empty()) {
    // Sim is already unlocked.
    failure_callback.Run(networking_private::kErrorInvalidNetworkOperation);
    return;
  }

  // Unblock or unlock the SIM.
  if (lock_type == shill::kSIMLockPuk) {
    NetworkHandler::Get()->network_device_handler()->UnblockPin(
        device_state->path(), puk, pin, success_callback,
        base::Bind(&NetworkHandlerFailureCallback, failure_callback));
  } else {
    NetworkHandler::Get()->network_device_handler()->EnterPin(
        device_state->path(), pin, success_callback,
        base::Bind(&NetworkHandlerFailureCallback, failure_callback));
  }
}

void NetworkingPrivateChromeOS::SetCellularSimState(
    const std::string& guid,
    bool require_pin,
    const std::string& current_pin,
    const std::string& new_pin,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  const chromeos::DeviceState* device_state = GetCellularDeviceState(guid);
  if (!device_state) {
    failure_callback.Run(networking_private::kErrorNetworkUnavailable);
    return;
  }
  if (!device_state->sim_lock_type().empty()) {
    // The SIM needs to be unlocked before the state can be changed.
    failure_callback.Run(networking_private::kErrorSimLocked);
    return;
  }

  // Only set a new pin if require_pin is true.
  std::string set_new_pin = require_pin ? new_pin : "";
  NetworkHandler::Get()->network_device_handler()->RequirePin(
      device_state->path(), require_pin, current_pin,
      base::Bind(&RequirePinSuccess, device_state->path(), current_pin,
                 set_new_pin, success_callback, failure_callback),
      base::Bind(&NetworkHandlerFailureCallback, failure_callback));
}

std::unique_ptr<base::ListValue>
NetworkingPrivateChromeOS::GetEnabledNetworkTypes() {
  chromeos::NetworkStateHandler* state_handler = GetStateHandler();

  std::unique_ptr<base::ListValue> network_list(new base::ListValue);

  if (state_handler->IsTechnologyEnabled(NetworkTypePattern::Ethernet()))
    network_list->AppendString(::onc::network_type::kEthernet);
  if (state_handler->IsTechnologyEnabled(NetworkTypePattern::WiFi()))
    network_list->AppendString(::onc::network_type::kWiFi);
  if (state_handler->IsTechnologyEnabled(NetworkTypePattern::Wimax()))
    network_list->AppendString(::onc::network_type::kWimax);
  if (state_handler->IsTechnologyEnabled(NetworkTypePattern::Cellular()))
    network_list->AppendString(::onc::network_type::kCellular);

  return network_list;
}

std::unique_ptr<NetworkingPrivateDelegate::DeviceStateList>
NetworkingPrivateChromeOS::GetDeviceStateList() {
  std::set<std::string> technologies_found;
  NetworkStateHandler::DeviceStateList devices;
  NetworkHandler::Get()->network_state_handler()->GetDeviceList(&devices);

  std::unique_ptr<DeviceStateList> device_state_list(new DeviceStateList);
  for (const chromeos::DeviceState* device : devices) {
    std::string onc_type =
        chromeos::network_util::TranslateShillTypeToONC(device->type());
    AppendDeviceState(onc_type, device, device_state_list.get());
    technologies_found.insert(onc_type);
  }

  // For any technologies that we do not have a DeviceState entry for, append
  // an entry if the technology is available.
  const char* technology_types[] = {
      ::onc::network_type::kEthernet, ::onc::network_type::kWiFi,
      ::onc::network_type::kWimax, ::onc::network_type::kCellular};
  for (const char* technology : technology_types) {
    if (base::ContainsValue(technologies_found, technology))
      continue;
    AppendDeviceState(technology, nullptr /* device */,
                      device_state_list.get());
  }
  return device_state_list;
}

std::unique_ptr<base::DictionaryValue>
NetworkingPrivateChromeOS::GetGlobalPolicy() {
  auto result = base::MakeUnique<base::DictionaryValue>();
  const base::DictionaryValue* global_network_config =
      GetManagedConfigurationHandler()->GetGlobalConfigFromPolicy(
          std::string() /* no username hash, device policy */);
  if (global_network_config)
    result->MergeDictionary(global_network_config);
  return result;
}

bool NetworkingPrivateChromeOS::EnableNetworkType(const std::string& type) {
  NetworkTypePattern pattern =
      chromeos::onc::NetworkTypePatternFromOncType(type);

  GetStateHandler()->SetTechnologyEnabled(
      pattern, true, chromeos::network_handler::ErrorCallback());

  return true;
}

bool NetworkingPrivateChromeOS::DisableNetworkType(const std::string& type) {
  NetworkTypePattern pattern =
      chromeos::onc::NetworkTypePatternFromOncType(type);

  GetStateHandler()->SetTechnologyEnabled(
      pattern, false, chromeos::network_handler::ErrorCallback());

  return true;
}

bool NetworkingPrivateChromeOS::RequestScan() {
  GetStateHandler()->RequestScan();
  return true;
}

// Private methods

void NetworkingPrivateChromeOS::GetPropertiesCallback(
    const std::string& guid,
    bool managed,
    const DictionaryCallback& callback,
    const std::string& service_path,
    const base::DictionaryValue& dictionary) {
  std::unique_ptr<base::DictionaryValue> dictionary_copy(dictionary.DeepCopy());
  AppendThirdPartyProviderName(dictionary_copy.get());
  if (managed)
    SetManagedActiveProxyValues(guid, dictionary_copy.get());
  callback.Run(std::move(dictionary_copy));
}

void NetworkingPrivateChromeOS::AppendThirdPartyProviderName(
    base::DictionaryValue* dictionary) {
  base::DictionaryValue* third_party_vpn =
      GetThirdPartyVPNDictionary(dictionary);
  if (!third_party_vpn)
    return;

  const std::string extension_id = GetStringFromDictionary(
      *third_party_vpn, ::onc::third_party_vpn::kExtensionID);
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(browser_context_)->enabled_extensions();
  for (const auto& extension : extensions) {
    if (extension->permissions_data()->HasAPIPermission(
            APIPermission::kVpnProvider) &&
        extension->id() == extension_id) {
      third_party_vpn->SetStringWithoutPathExpansion(
          ::onc::third_party_vpn::kProviderName, extension->name());
      break;
    }
  }
}

void NetworkingPrivateChromeOS::SetManagedActiveProxyValues(
    const std::string& guid,
    base::DictionaryValue* dictionary) {
  // NOTE: We use UIProxyConfigService and UIProxyConfig for historical
  // reasons. The model and service were written for a much older UI but
  // contain a fair amount of subtle logic which is why we use them.
  // TODO(stevenjb): Re-factor this code and eliminate UIProxyConfig once
  // the old options UI is abandoned. crbug.com/662529.
  chromeos::UIProxyConfigService* ui_proxy_config_service =
      NetworkHandler::Get()->ui_proxy_config_service();
  ui_proxy_config_service->UpdateFromPrefs(guid);
  UIProxyConfig config;
  ui_proxy_config_service->GetProxyConfig(guid, &config);
  ProxyPrefs::ConfigState state = config.state;

  VLOG(1) << "CONFIG STATE FOR: " << guid << ": " << state
          << " MODE: " << config.mode;

  if (state != ProxyPrefs::CONFIG_POLICY &&
      state != ProxyPrefs::CONFIG_EXTENSION &&
      state != ProxyPrefs::CONFIG_OTHER_PRECEDE) {
    return;
  }

  // Ensure that the ProxySettings dictionary exists.
  base::DictionaryValue* proxy_settings =
      EnsureDictionaryValue(::onc::network_config::kProxySettings, dictionary);
  VLOG(2) << " PROXY: " << *proxy_settings;

  // Ensure that the ProxySettings.Type dictionary exists and set the Type
  // value to the value from |ui_proxy_config_service|.
  base::DictionaryValue* proxy_type_dict =
      EnsureDictionaryValue(::onc::network_config::kType, proxy_settings);
  SetProxyEffectiveValue(proxy_type_dict, state,
                         base::WrapUnique<base::Value>(new base::StringValue(
                             GetProxySettingsType(config.mode))));

  // Update any appropriate sub dictionary based on the new type.
  switch (config.mode) {
    case UIProxyConfig::MODE_SINGLE_PROXY: {
      // Use the same proxy value (config.single_proxy) for all proxies.
      base::DictionaryValue* manual =
          EnsureDictionaryValue(::onc::proxy::kManual, proxy_settings);
      SetManualProxy(manual, state, ::onc::proxy::kHttp, config.single_proxy);
      SetManualProxy(manual, state, ::onc::proxy::kHttps, config.single_proxy);
      SetManualProxy(manual, state, ::onc::proxy::kFtp, config.single_proxy);
      SetManualProxy(manual, state, ::onc::proxy::kSocks, config.single_proxy);
      break;
    }
    case UIProxyConfig::MODE_PROXY_PER_SCHEME: {
      base::DictionaryValue* manual =
          EnsureDictionaryValue(::onc::proxy::kManual, proxy_settings);
      SetManualProxy(manual, state, ::onc::proxy::kHttp, config.http_proxy);
      SetManualProxy(manual, state, ::onc::proxy::kHttps, config.https_proxy);
      SetManualProxy(manual, state, ::onc::proxy::kFtp, config.ftp_proxy);
      SetManualProxy(manual, state, ::onc::proxy::kSocks, config.socks_proxy);
      break;
    }
    case UIProxyConfig::MODE_PAC_SCRIPT: {
      base::DictionaryValue* pac =
          EnsureDictionaryValue(::onc::proxy::kPAC, proxy_settings);
      SetProxyEffectiveValue(
          pac, state, base::WrapUnique<base::Value>(new base::StringValue(
                          config.automatic_proxy.pac_url.spec())));
      break;
    }
    case UIProxyConfig::MODE_DIRECT:
    case UIProxyConfig::MODE_AUTO_DETECT:
      break;
  }

  VLOG(2) << " NEW PROXY: " << *proxy_settings;
}

void NetworkingPrivateChromeOS::ConnectFailureCallback(
    const std::string& guid,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  // TODO(stevenjb): Temporary workaround to show the configuration UI.
  // Eventually the caller (e.g. Settings) should handle any failures and
  // show its own configuration UI. crbug.com/380937.
  if (ui_delegate()->HandleConnectFailed(guid, error_name)) {
    success_callback.Run();
    return;
  }
  failure_callback.Run(error_name);
}

}  // namespace extensions
