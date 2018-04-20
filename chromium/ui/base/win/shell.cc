// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/shell.h"

#include <dwmapi.h>
#include <shlobj.h>  // Must be before propkey.
#include <propkey.h>
#include <shellapi.h>

#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/native_library.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/scoped_comptr.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "ui/base/ui_base_switches.h"

namespace ui {
namespace win {

namespace {

// Default ShellExecuteEx flags used with "openas", "explore", and default
// verbs.
//
// SEE_MASK_NOASYNC is specified so that ShellExecuteEx can be invoked from a
// thread whose message loop may not wait around long enough for the
// asynchronous tasks initiated by ShellExecuteEx to complete. Using this flag
// causes ShellExecuteEx() to block until these tasks complete.
const DWORD kDefaultShellExecuteFlags = SEE_MASK_NOASYNC;

// Invokes ShellExecuteExW() with the given parameters.
bool InvokeShellExecute(const base::string16 path,
                         const base::string16 working_directory,
                         const base::string16 args,
                         const base::string16 verb,
                         DWORD mask) {
  base::ThreadRestrictions::AssertIOAllowed();
  SHELLEXECUTEINFO sei = {sizeof(sei)};
  sei.fMask = mask;
  sei.nShow = SW_SHOWNORMAL;
  sei.lpVerb = (verb.empty() ? nullptr : verb.c_str());
  sei.lpFile = path.c_str();
  sei.lpDirectory =
      (working_directory.empty() ? nullptr : working_directory.c_str());
  sei.lpParameters = (args.empty() ? nullptr : args.c_str());
  return ::ShellExecuteExW(&sei);
}

}  // namespace

bool OpenAnyViaShell(const base::string16& full_path,
                     const base::string16& directory,
                     const base::string16& args,
                     DWORD mask) {
  return InvokeShellExecute(full_path, directory, args, base::string16(), mask);
}

bool OpenFileViaShell(const base::FilePath& full_path) {
  // Invoke the default verb on the file with no arguments.
  return InvokeShellExecute(full_path.value(), full_path.DirName().value(),
                            base::string16(), base::string16(),
                            kDefaultShellExecuteFlags);
}

bool OpenFolderViaShell(const base::FilePath& full_path) {
  // The "explore" verb causes the folder at |full_path| to be displayed in a
  // file browser. This will fail if |full_path| is not a directory.
  return InvokeShellExecute(full_path.value(), full_path.value(),
                            base::string16(), L"explore",
                            kDefaultShellExecuteFlags);
}

bool PreventWindowFromPinning(HWND hwnd) {
  DCHECK(hwnd);

  // This functionality is only available on Win7+.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return false;

  base::win::ScopedComPtr<IPropertyStore> pps;
  if (FAILED(SHGetPropertyStoreForWindow(hwnd,
                                         IID_PPV_ARGS(pps.Receive()))))
    return false;

  return base::win::SetBooleanValueForPropertyStore(
      pps.get(), PKEY_AppUserModel_PreventPinning, true);
}

// TODO(calamity): investigate moving this out of the UI thread as COM
// operations may spawn nested message loops which can cause issues.
void SetAppDetailsForWindow(const base::string16& app_id,
                            const base::FilePath& app_icon_path,
                            int app_icon_index,
                            const base::string16& relaunch_command,
                            const base::string16& relaunch_display_name,
                            HWND hwnd) {
  DCHECK(hwnd);

  // This functionality is only available on Win7+.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  base::win::ScopedComPtr<IPropertyStore> pps;
  if (FAILED(SHGetPropertyStoreForWindow(hwnd,
                                         IID_PPV_ARGS(pps.Receive()))))
    return;

  if (!app_id.empty())
    base::win::SetAppIdForPropertyStore(pps.get(), app_id.c_str());
  if (!app_icon_path.empty()) {
    // Always add the icon index explicitly to prevent bad interaction with the
    // index notation when file path has commas.
    base::win::SetStringValueForPropertyStore(
        pps.get(), PKEY_AppUserModel_RelaunchIconResource,
        base::StringPrintf(L"%ls,%d", app_icon_path.value().c_str(),
                           app_icon_index)
            .c_str());
  }
  if (!relaunch_command.empty()) {
    base::win::SetStringValueForPropertyStore(
        pps.get(), PKEY_AppUserModel_RelaunchCommand,
        relaunch_command.c_str());
  }
  if (!relaunch_display_name.empty()) {
    base::win::SetStringValueForPropertyStore(
        pps.get(), PKEY_AppUserModel_RelaunchDisplayNameResource,
        relaunch_display_name.c_str());
  }
}

void SetAppIdForWindow(const base::string16& app_id, HWND hwnd) {
  SetAppDetailsForWindow(app_id, base::FilePath(), 0, base::string16(),
                         base::string16(), hwnd);
}

void SetAppIconForWindow(const base::FilePath& app_icon_path,
                         int app_icon_index,
                         HWND hwnd) {
  SetAppDetailsForWindow(base::string16(), app_icon_path, app_icon_index,
                         base::string16(), base::string16(), hwnd);
}

void SetRelaunchDetailsForWindow(const base::string16& relaunch_command,
                                 const base::string16& display_name,
                                 HWND hwnd) {
  SetAppDetailsForWindow(base::string16(), base::FilePath(), 0,
                         relaunch_command, display_name, hwnd);
}

void ClearWindowPropertyStore(HWND hwnd) {
  DCHECK(hwnd);

  // This functionality is only available on Win7+.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  base::win::ScopedComPtr<IPropertyStore> pps;
  if (FAILED(SHGetPropertyStoreForWindow(hwnd,
                                         IID_PPV_ARGS(pps.Receive()))))
    return;

  DWORD property_count;
  if (FAILED(pps->GetCount(&property_count)))
    return;

  PROPVARIANT empty_property_variant = {};
  for (DWORD i = 0; i < property_count; i++) {
    PROPERTYKEY key;
    if (SUCCEEDED(pps->GetAt(i, &key)))
      pps->SetValue(key, empty_property_variant);
  }

  pps->Commit();
}

bool IsAeroGlassEnabled() {
  // For testing in Win8 (where it is not possible to disable composition) the
  // user can specify this command line switch to mimic the behavior.  In this
  // mode, cross-HWND transparency is not supported and various types of
  // widgets fallback to more simplified rendering behavior.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableDwmComposition))
    return false;

  // Technically Aero glass works in Vista but we want to put XP and Vista
  // at the same feature level. See bug 426573.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return false;
  // If composition is not enabled, we behave like on XP.
  BOOL enabled = FALSE;
  return SUCCEEDED(DwmIsCompositionEnabled(&enabled)) && enabled;
}

}  // namespace win
}  // namespace ui
