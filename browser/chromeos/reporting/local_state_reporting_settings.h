// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_REPORTING_LOCAL_STATE_REPORTING_SETTINGS_H_
#define CHROME_BROWSER_CHROMEOS_REPORTING_LOCAL_STATE_REPORTING_SETTINGS_H_

#include <map>
#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/reporting/metrics/reporting_settings.h"

namespace reporting {

// `LocalStateReportingSettings` is used to fetch reporting settings via
// local_state's pref store. Extends a convenient common interface that metric
// reporting components use to access reporting settings and subscribe to
// reporting setting updates. Needs to be accessed from the main thread given
// its dependency on local_state's pref store.
class LocalStateReportingSettings : public ReportingSettings {
 public:
  LocalStateReportingSettings();
  LocalStateReportingSettings(const LocalStateReportingSettings& other) =
      delete;
  LocalStateReportingSettings& operator=(
      const LocalStateReportingSettings& other) = delete;
  ~LocalStateReportingSettings() override;

  // ReportingSettings:
  base::CallbackListSubscription AddSettingsObserver(
      const std::string& path,
      base::RepeatingClosure callback) override;
  bool PrepareTrustedValues(base::OnceClosure callback) override;
  bool GetBoolean(const std::string& path, bool* out_value) const override;
  bool GetInteger(const std::string& path, int* out_value) const override;
  bool GetList(const std::string& path,
               const base::Value::List** out_value) const override;

  // Only bool and List are allowed, otherwise will return false.
  bool GetReportingEnabled(const std::string& path,
                           bool* out_value) const override;

  // Returns whether the specified setting is being observed for testing
  // purposes.
  bool IsObservingSettingsForTest(const std::string& path);

 private:
  // Internal callback triggered when the setting value is updated.
  void OnPrefChanged(const std::string& path);

  SEQUENCE_CHECKER(sequence_checker_);

  PrefChangeRegistrar pref_change_registrar_;

  // A map of setting names to a list of observers. Observers are triggered in
  // the order they are added.
  std::map<std::string, std::unique_ptr<base::RepeatingClosureList>>
      settings_observers_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<LocalStateReportingSettings> weak_ptr_factory_{this};
};

}  // namespace reporting

#endif  // CHROME_BROWSER_CHROMEOS_REPORTING_LOCAL_STATE_REPORTING_SETTINGS_H_
