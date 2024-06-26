// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/legacy_password_store_backend_migration_decorator.h"

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using ::testing::Return;
using ::testing::WithArg;
using ::testing::WithArgs;
using MigrationType = BuiltInBackendToAndroidBackendMigrator::MigrationType;

constexpr double kLastMigrationAttemptTime = 0.0;

}  // namespace

class LegacyPasswordStoreBackendMigrationDecoratorTest : public testing::Test {
 protected:
  LegacyPasswordStoreBackendMigrationDecoratorTest() {
    prefs_.registry()->RegisterIntegerPref(
        prefs::kCurrentMigrationVersionToGoogleMobileServices, 0);
    prefs_.registry()->RegisterDoublePref(prefs::kTimeOfLastMigrationAttempt,
                                          kLastMigrationAttemptTime);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kRequiresMigrationAfterSyncStatusChange, false);
    prefs_.registry()->RegisterStringPref(
        ::prefs::kGoogleServicesLastSyncingUsername, "testaccount@gmail.com");
    prefs_.registry()->RegisterBooleanPref(
        prefs::kUnenrolledFromGoogleMobileServicesDueToErrors, false);
    prefs_.registry()->RegisterIntegerPref(
        password_manager::prefs::kTimesReenrolledToGoogleMobileServices, 0);
    prefs_.registry()->RegisterIntegerPref(
        password_manager::prefs::
            kTimesAttemptedToReenrollToGoogleMobileServices,
        0);
    prefs_.registry()->RegisterIntegerPref(
        prefs::kPasswordsUseUPMLocalAndSeparateStores,
        static_cast<int>(
            password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOff));

    backend_migration_decorator_ =
        std::make_unique<LegacyPasswordStoreBackendMigrationDecorator>(
            CreateBuiltInBackend(), CreateAndroidBackend(), &prefs_);
  }

  ~LegacyPasswordStoreBackendMigrationDecoratorTest() override {
    backend_migration_decorator()->Shutdown(base::DoNothing());
  }

  void InitSyncService(bool is_password_sync_enabled) {
    if (is_password_sync_enabled) {
      sync_service_.GetUserSettings()->SetSelectedTypes(
          /*sync_everything=*/false,
          /*types=*/{syncer::UserSelectableType::kPasswords});
    } else {
      sync_service_.GetUserSettings()->SetSelectedTypes(
          /*sync_everything=*/false, /*types=*/{});
    }
    backend_migration_decorator()->OnSyncServiceInitialized(&sync_service_);
  }

  void ChangeSyncSetting(bool is_password_sync_enabled) {
    if (is_password_sync_enabled) {
      sync_service_.GetUserSettings()->SetSelectedTypes(
          /*sync_everything=*/false,
          /*types=*/{syncer::UserSelectableType::kPasswords});
    } else {
      sync_service_.GetUserSettings()->SetSelectedTypes(
          /*sync_everything=*/false, /*types=*/{});
    }
    sync_service_.FireStateChanged();
    RunUntilIdle();
  }

  PasswordStoreBackend* backend_migration_decorator() {
    return backend_migration_decorator_.get();
  }
  MockPasswordStoreBackend* built_in_backend() { return built_in_backend_; }
  MockPasswordStoreBackend* android_backend() { return android_backend_; }

  TestingPrefServiceSimple& prefs() { return prefs_; }

  syncer::TestSyncService& sync_service() { return sync_service_; }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

  void FastForwardUntilNoTasksRemain() {
    task_env_.FastForwardUntilNoTasksRemain();
  }

  MigrationType migration_in_progress_type() const {
    return backend_migration_decorator_->migration_in_progress_type();
  }

 private:
  std::unique_ptr<PasswordStoreBackend> CreateBuiltInBackend() {
    auto unique_backend = std::make_unique<MockPasswordStoreBackend>();
    built_in_backend_ = unique_backend.get();
    return unique_backend;
  }

  std::unique_ptr<PasswordStoreBackend> CreateAndroidBackend() {
    auto unique_backend = std::make_unique<MockPasswordStoreBackend>();
    android_backend_ = unique_backend.get();
    return unique_backend;
  }

  base::test::SingleThreadTaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple prefs_;
  raw_ptr<MockPasswordStoreBackend> built_in_backend_;
  raw_ptr<MockPasswordStoreBackend> android_backend_;
  syncer::TestSyncService sync_service_;

  std::unique_ptr<LegacyPasswordStoreBackendMigrationDecorator>
      backend_migration_decorator_;
};

TEST_F(LegacyPasswordStoreBackendMigrationDecoratorTest,
       OnSyncServiceInitializedPropagatedToAndroidBackend) {
  EXPECT_CALL(*android_backend(), OnSyncServiceInitialized(&sync_service()));
  backend_migration_decorator()->OnSyncServiceInitialized(&sync_service());
}

TEST_F(
    LegacyPasswordStoreBackendMigrationDecoratorTest,
    NonSyncableDataMigrationStartsWithoutRelaunchWhenSyncEnabledBeforeStoreSplit) {
  base::MockCallback<base::OnceCallback<void(bool)>> mock_completion_callback;
  base::RepeatingClosure sync_status_changed_closure;

  // Init backend.
  EXPECT_CALL(mock_completion_callback, Run(/*success=*/true));
  EXPECT_CALL(*built_in_backend(), InitBackend)
      .WillOnce(WithArgs<2, 3>(
          [&sync_status_changed_closure](auto sync_status_changed,
                                         auto completion_callback) {
            std::move(completion_callback).Run(/*success=*/true);
            // Capture |sync_enabled_or_disabled_cb| passed to the
            // build_in_backend.
            sync_status_changed_closure = std::move(sync_status_changed);
          }));
  EXPECT_CALL(*android_backend(), InitBackend)
      .WillOnce(WithArg<3>([](auto completion_callback) {
        std::move(completion_callback).Run(/*success=*/true);
      }));
  backend_migration_decorator()->InitBackend(
      /*affiliated_match_helper=*/nullptr,
      /*remote_form_changes_received=*/base::DoNothing(),
      /*sync_enabled_or_disabled_cb=*/base::DoNothing(),
      /*completion=*/mock_completion_callback.Get());

  InitSyncService(/*is_password_sync_enabled=*/false);

  // Enable password sync in settings.
  ChangeSyncSetting(/*is_password_sync_enabled=*/true);

  // Migration of non-syncable data to the android backend will trigger
  // login retrieval from the built-in backend first.
  EXPECT_CALL(*built_in_backend(), GetAllLoginsAsync);
  EXPECT_CALL(*android_backend(), GetAllLoginsAsync).Times(0);

  // Invoke sync callback to simulate a change in sync status.
  sync_status_changed_closure.Run();
  RunUntilIdle();
  EXPECT_EQ(MigrationType::kNonSyncableToAndroidBackend,
            migration_in_progress_type());
}

TEST_F(
    LegacyPasswordStoreBackendMigrationDecoratorTest,
    NonSyncableDataMigrationStartsWithoutRelaunchWhenSyncBecomesDisabledBeforeStoreSplit) {
  // Init backend.
  base::MockCallback<base::OnceCallback<void(bool)>> mock_completion_callback;
  base::RepeatingClosure sync_status_changed_closure;
  EXPECT_CALL(mock_completion_callback, Run(/*success=*/true));
  EXPECT_CALL(*built_in_backend(), InitBackend)
      .WillOnce(WithArgs<2, 3>(
          [&sync_status_changed_closure](auto sync_status_changed,
                                         auto completion_callback) {
            std::move(completion_callback).Run(/*success=*/true);
            // Capture |sync_enabled_or_disabled_cb| passed to the
            // build_in_backend.
            sync_status_changed_closure = std::move(sync_status_changed);
          }));
  EXPECT_CALL(*android_backend(), InitBackend)
      .WillOnce(WithArg<3>([](auto completion_callback) {
        std::move(completion_callback).Run(/*success=*/true);
      }));
  backend_migration_decorator()->InitBackend(
      /*affiliated_match_helper=*/nullptr,
      /*remote_form_changes_received=*/base::DoNothing(),
      /*sync_enabled_or_disabled_cb=*/base::DoNothing(),
      /*completion=*/mock_completion_callback.Get());

  InitSyncService(/*is_password_sync_enabled=*/true);
  ChangeSyncSetting(/*is_password_sync_enabled=*/false);

  // Invoke sync callback to simulate appliying new setting. Set expectation
  // for sync to be turned off.
  // Migration of non-syncable data to the built-in backend will trigger
  // retrieval of logins for the last sync account from the android backend
  // first.
  EXPECT_CALL(*built_in_backend(), GetAllLoginsAsync).Times(0);
  EXPECT_CALL(*android_backend(), GetAllLoginsForAccountAsync);

  sync_status_changed_closure.Run();
  RunUntilIdle();
  EXPECT_EQ(MigrationType::kNonSyncableToBuiltInBackend,
            migration_in_progress_type());
}

TEST_F(LegacyPasswordStoreBackendMigrationDecoratorTest,
       NonSyncableDataMigrationToBuiltInBackendResetsInitialMigrationStatus) {
  prefs().SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices, 1);

  // Init backend.
  base::MockCallback<base::OnceCallback<void(bool)>> mock_completion_callback;
  base::RepeatingClosure sync_status_changed_closure;
  EXPECT_CALL(mock_completion_callback, Run(/*success=*/true));
  EXPECT_CALL(*built_in_backend(), InitBackend)
      .WillOnce(WithArgs<2, 3>(
          [&sync_status_changed_closure](auto sync_status_changed,
                                         auto completion_callback) {
            std::move(completion_callback).Run(/*success=*/true);
            // Capture |sync_enabled_or_disabled_cb| passed to the
            // build_in_backend.
            sync_status_changed_closure = std::move(sync_status_changed);
          }));
  EXPECT_CALL(*android_backend(), InitBackend)
      .WillOnce(WithArg<3>([](auto completion_callback) {
        std::move(completion_callback).Run(/*success=*/true);
      }));
  backend_migration_decorator()->InitBackend(
      /*affiliated_match_helper=*/nullptr,
      /*remote_form_changes_received=*/base::DoNothing(),
      /*sync_enabled_or_disabled_cb=*/base::DoNothing(),
      /*completion=*/mock_completion_callback.Get());

  InitSyncService(/*is_password_sync_enabled=*/true);
  ChangeSyncSetting(/*is_password_sync_enabled=*/false);

  // Invoke sync callback to simulate appliying new setting. Set expectation
  // for sync to be turned off.
  sync_status_changed_closure.Run();
  RunUntilIdle();

  EXPECT_EQ(MigrationType::kNonSyncableToBuiltInBackend,
            migration_in_progress_type());
  // Check that the initial migration status is reset.
  EXPECT_EQ(0, prefs().GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
}

TEST_F(
    LegacyPasswordStoreBackendMigrationDecoratorTest,
    ResetAutoSignInWhenInitBackendAfterSyncWasDisabledButSettingWasNotApplied) {
  prefs().SetBoolean(prefs::kRequiresMigrationAfterSyncStatusChange, true);

  // Init backend.
  base::MockCallback<base::OnceCallback<void(bool)>> mock_completion_callback;
  base::RepeatingClosure sync_status_changed_closure;
  EXPECT_CALL(mock_completion_callback, Run(/*success=*/true));
  EXPECT_CALL(*built_in_backend(), InitBackend)
      .WillOnce(WithArgs<2, 3>(
          [&sync_status_changed_closure](auto sync_status_changed,
                                         auto completion_callback) {
            std::move(completion_callback).Run(/*success=*/true);
            // Capture |sync_enabled_or_disabled_cb| passed to the
            // build_in_backend.
            sync_status_changed_closure = std::move(sync_status_changed);
          }));
  EXPECT_CALL(*android_backend(), InitBackend)
      .WillOnce(WithArg<3>([](auto completion_callback) {
        std::move(completion_callback).Run(/*success=*/true);
      }));
  backend_migration_decorator()->InitBackend(
      /*affiliated_match_helper=*/nullptr,
      /*remote_form_changes_received=*/base::DoNothing(),
      /*sync_enabled_or_disabled_cb=*/base::DoNothing(),
      /*completion=*/mock_completion_callback.Get());

  // Set expectation for sync to be turned off.
  InitSyncService(/*is_password_sync_enabled=*/false);

  // Expect that autosignin will be disabled for logins in the buil-in backend.
  EXPECT_CALL(*built_in_backend(), DisableAutoSignInForOriginsAsync);
  FastForwardUntilNoTasksRemain();

  EXPECT_EQ(false,
            prefs().GetBoolean(prefs::kRequiresMigrationAfterSyncStatusChange));
}

TEST_F(
    LegacyPasswordStoreBackendMigrationDecoratorTest,
    DoNotResetAutoSignInWhenInitBackendAfterSyncEnabledButSettingWasNotApplied) {
  prefs().SetBoolean(prefs::kRequiresMigrationAfterSyncStatusChange, true);

  // Init backend.
  base::MockCallback<base::OnceCallback<void(bool)>> mock_completion_callback;
  base::RepeatingClosure sync_status_changed_closure;
  EXPECT_CALL(mock_completion_callback, Run(/*success=*/true));
  EXPECT_CALL(*built_in_backend(), InitBackend)
      .WillOnce(WithArgs<2, 3>(
          [&sync_status_changed_closure](auto sync_status_changed,
                                         auto completion_callback) {
            std::move(completion_callback).Run(/*success=*/true);
            // Capture |sync_enabled_or_disabled_cb| passed to the
            // build_in_backend.
            sync_status_changed_closure = std::move(sync_status_changed);
          }));
  EXPECT_CALL(*android_backend(), InitBackend)
      .WillOnce(WithArg<3>([](auto completion_callback) {
        std::move(completion_callback).Run(/*success=*/true);
      }));
  backend_migration_decorator()->InitBackend(
      /*affiliated_match_helper=*/nullptr,
      /*remote_form_changes_received=*/base::DoNothing(),
      /*sync_enabled_or_disabled_cb=*/base::DoNothing(),
      /*completion=*/mock_completion_callback.Get());

  // Set expectation for sync to be turned off.
  InitSyncService(/*is_password_sync_enabled=*/true);

  // Expect that autosignin will be disabled for logins in the buil-in backend.
  EXPECT_CALL(*built_in_backend(), DisableAutoSignInForOriginsAsync).Times(0);
  FastForwardUntilNoTasksRemain();

  EXPECT_EQ(true,
            prefs().GetBoolean(prefs::kRequiresMigrationAfterSyncStatusChange));
}

TEST_F(LegacyPasswordStoreBackendMigrationDecoratorTest,
       NonSyncableDataMigrationHappensOnlyOnceOnMultipleSyncStatusChanges) {
  // Init backend.
  base::MockCallback<base::OnceCallback<void(bool)>> mock_completion_callback;
  base::RepeatingClosure sync_status_changed_closure;
  EXPECT_CALL(mock_completion_callback, Run(/*success=*/true));
  EXPECT_CALL(*built_in_backend(), InitBackend)
      .WillOnce(WithArgs<2, 3>(
          [&sync_status_changed_closure](auto sync_status_changed,
                                         auto completion_callback) {
            std::move(completion_callback).Run(/*success=*/true);
            // Capture |sync_enabled_or_disabled_cb| passed to the
            // build_in_backend.
            sync_status_changed_closure = std::move(sync_status_changed);
          }));
  EXPECT_CALL(*android_backend(), InitBackend)
      .WillOnce(WithArg<3>([](auto completion_callback) {
        std::move(completion_callback).Run(/*success=*/true);
      }));
  backend_migration_decorator()->InitBackend(
      /*affiliated_match_helper=*/nullptr,
      /*remote_form_changes_received=*/base::DoNothing(),
      /*sync_enabled_or_disabled_cb=*/base::DoNothing(),
      /*completion=*/mock_completion_callback.Get());

  InitSyncService(/*is_password_sync_enabled=*/true);

  ChangeSyncSetting(/*is_password_sync_enabled=*/false);
  // Invoke sync callback to simulate appliying new setting. Set expectation
  // for sync to be turned off.
  // Migration of non-syncable data to the built-in backend will trigger
  // retrieval of logins for the last sync account from the android backend
  // first.
  EXPECT_CALL(*built_in_backend(), GetAllLoginsAsync).Times(0);
  // Imitate android backend retrieving logins for migration, do not proceed
  // with updating logins in the built-in backend to imitate migration running.
  // (This test uses mock backends, so unless a callback from updating a login
  // in the built-in backend is imitated, migration is not finished.)
  EXPECT_CALL(*android_backend(), GetAllLoginsForAccountAsync)
      .WillOnce(
          WithArg<1>(testing::Invoke([](LoginsOrErrorReply reply) -> void {
            LoginsResult logins;
            logins.emplace_back();
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(std::move(reply), std::move(logins)));
          })));

  sync_status_changed_closure.Run();
  RunUntilIdle();
  EXPECT_EQ(MigrationType::kNonSyncableToBuiltInBackend,
            migration_in_progress_type());

  // Check that the migration is not finished yet by querying the migration
  // version pref that was reset on sync settings change, and will be updated
  // only when the migration finishes.
  EXPECT_EQ(0, prefs().GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));

  ChangeSyncSetting(/*is_password_sync_enabled=*/true);
  // Invoke sync callback to simulate appliying new setting. Set expectation
  // for sync to be turned on.
  // Migration of non-syncable data should not start, as the previous
  // migration attempt is still running.
  EXPECT_CALL(*built_in_backend(), GetAllLoginsAsync).Times(0);
  EXPECT_CALL(*android_backend(), GetAllLoginsForAccountAsync).Times(0);
  sync_status_changed_closure.Run();
  RunUntilIdle();
}

TEST_F(LegacyPasswordStoreBackendMigrationDecoratorTest,
       NonSyncableDataMigrationDoesNotStartForUsersUnenrolledFromUPM) {
  prefs().SetBoolean(prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
                     true);
  base::MockCallback<base::OnceCallback<void(bool)>> mock_completion_callback;
  base::RepeatingClosure sync_status_changed_closure;

  // Init backend.
  EXPECT_CALL(mock_completion_callback, Run(/*success=*/true));
  EXPECT_CALL(*built_in_backend(), InitBackend)
      .WillOnce(WithArgs<2, 3>(
          [&sync_status_changed_closure](auto sync_status_changed,
                                         auto completion_callback) {
            std::move(completion_callback).Run(/*success=*/true);
            // Capture |sync_enabled_or_disabled_cb| passed to the
            // build_in_backend.
            sync_status_changed_closure = std::move(sync_status_changed);
          }));
  EXPECT_CALL(*android_backend(), InitBackend)
      .WillOnce(WithArg<3>([](auto completion_callback) {
        std::move(completion_callback).Run(/*success=*/true);
      }));
  backend_migration_decorator()->InitBackend(
      /*affiliated_match_helper=*/nullptr,
      /*remote_form_changes_received=*/base::DoNothing(),
      /*sync_enabled_or_disabled_cb=*/base::DoNothing(),
      /*completion=*/mock_completion_callback.Get());

  InitSyncService(/*is_password_sync_enabled=*/false);

  // Enable password sync in settings.
  ChangeSyncSetting(/*is_password_sync_enabled=*/true);

  // Migration of non-syncable data to the android backend will not start and
  // therefore will not trigger any logins retrieval.
  EXPECT_CALL(*built_in_backend(), GetAllLoginsAsync).Times(0);
  EXPECT_CALL(*android_backend(), GetAllLoginsAsync).Times(0);

  // Invoke sync callback to simulate a change in sync status.
  sync_status_changed_closure.Run();
  RunUntilIdle();
  EXPECT_EQ(MigrationType::kNone, migration_in_progress_type());

  // Verify that migration attempt did not happen by checking that the time of
  // the last migration attempt did not change.
  EXPECT_EQ(
      prefs().GetDouble(password_manager::prefs::kTimeOfLastMigrationAttempt),
      0.0);
}

TEST_F(LegacyPasswordStoreBackendMigrationDecoratorTest,
       ReenrollmentAttemptStartsForUsersUnenrolledFromUPM) {
  prefs().SetBoolean(prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
                     true);
  base::MockCallback<base::OnceCallback<void(bool)>> mock_completion_callback;

  // Init backend.
  EXPECT_CALL(*built_in_backend(), InitBackend);
  EXPECT_CALL(*android_backend(), InitBackend);
  backend_migration_decorator()->InitBackend(
      /*affiliated_match_helper=*/nullptr,
      /*remote_form_changes_received=*/base::DoNothing(),
      /*sync_enabled_or_disabled_cb=*/base::DoNothing(),
      /*completion=*/mock_completion_callback.Get());

  // Set password sync to be active and have no auth errors.
  InitSyncService(/*is_password_sync_enabled=*/true);
  sync_service().GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kPasswords});
  sync_service().ClearAuthError();

  // Migration attemot will start and will trigger logins retrieval from the
  // built-in backend.
  EXPECT_CALL(*built_in_backend(), GetAllLoginsAsync);
  EXPECT_CALL(*android_backend(), GetAllLoginsAsync).Times(0);

  // Imitate successfully completing a sync cycle.
  sync_service().FireSyncCycleCompleted();
  FastForwardUntilNoTasksRemain();
  EXPECT_EQ(MigrationType::kReenrollmentAttempt, migration_in_progress_type());

  // Verify that migration attempt happened by checking that the time of
  // the last migration attempt was updated.
  EXPECT_NE(
      prefs().GetDouble(password_manager::prefs::kTimeOfLastMigrationAttempt),
      kLastMigrationAttemptTime);
}

TEST_F(LegacyPasswordStoreBackendMigrationDecoratorTest,
       ReenrollmentAttemptDoesNotStartWhenSyncAuthErrorExists) {
  prefs().SetBoolean(prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
                     true);
  base::MockCallback<base::OnceCallback<void(bool)>> mock_completion_callback;

  // Init backend.
  EXPECT_CALL(*built_in_backend(), InitBackend);
  EXPECT_CALL(*android_backend(), InitBackend);
  backend_migration_decorator()->InitBackend(
      /*affiliated_match_helper=*/nullptr,
      /*remote_form_changes_received=*/base::DoNothing(),
      /*sync_enabled_or_disabled_cb=*/base::DoNothing(),
      /*completion=*/mock_completion_callback.Get());

  // Set password sync to be enabled in settings, but inactive.
  InitSyncService(/*is_password_sync_enabled=*/true);
  sync_service().GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, /*types=*/{});
  sync_service().SetPersistentAuthError();

  // Reenrolling migration attempt should not happen, logins should not be
  // retrieved.
  EXPECT_CALL(*built_in_backend(), GetAllLoginsAsync).Times(0);
  EXPECT_CALL(*android_backend(), GetAllLoginsAsync).Times(0);

  // Imitate successfully completing a sync cycle.
  sync_service().FireSyncCycleCompleted();
  FastForwardUntilNoTasksRemain();
  EXPECT_EQ(MigrationType::kNone, migration_in_progress_type());

  // Verify that migration attempt did not happen by checking that the time of
  // the last migration attempt did not change.
  EXPECT_EQ(
      prefs().GetDouble(password_manager::prefs::kTimeOfLastMigrationAttempt),
      kLastMigrationAttemptTime);
}

TEST_F(LegacyPasswordStoreBackendMigrationDecoratorTest,
       InitialUPMMigrationStartsForUsers) {
  // Init backend.
  EXPECT_CALL(*built_in_backend(), InitBackend);
  EXPECT_CALL(*android_backend(), InitBackend);
  backend_migration_decorator()->InitBackend(
      /*affiliated_match_helper=*/nullptr,
      /*remote_form_changes_received=*/base::DoNothing(),
      /*sync_enabled_or_disabled_cb=*/base::DoNothing(),
      /*completion=*/base::DoNothing());

  // Set password sync to be active and have no auth errors.
  InitSyncService(/*is_password_sync_enabled=*/true);

  // Migration attemot will start and will trigger logins retrieval from the
  // built-in backend.
  EXPECT_CALL(*built_in_backend(), GetAllLoginsAsync);
  EXPECT_CALL(*android_backend(), GetAllLoginsAsync).Times(0);

  // Imitate successfully completing a sync cycle.
  FastForwardUntilNoTasksRemain();
  EXPECT_EQ(MigrationType::kInitialForSyncUsers, migration_in_progress_type());
}

TEST_F(LegacyPasswordStoreBackendMigrationDecoratorTest,
       InitialUPMMigrationDoesNotStartsIfItWasDone) {
  prefs().SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices, 1);
  // Init backend.
  EXPECT_CALL(*built_in_backend(), InitBackend);
  EXPECT_CALL(*android_backend(), InitBackend);
  backend_migration_decorator()->InitBackend(
      /*affiliated_match_helper=*/nullptr,
      /*remote_form_changes_received=*/base::DoNothing(),
      /*sync_enabled_or_disabled_cb=*/base::DoNothing(),
      /*completion=*/base::DoNothing());

  // Set password sync to be active and have no auth errors.
  InitSyncService(/*is_password_sync_enabled=*/true);

  EXPECT_CALL(*built_in_backend(), GetAllLoginsAsync).Times(0);
  EXPECT_CALL(*android_backend(), GetAllLoginsAsync).Times(0);

  // Imitate successfully completing a sync cycle.
  FastForwardUntilNoTasksRemain();
  EXPECT_EQ(MigrationType::kNone, migration_in_progress_type());
}

TEST_F(LegacyPasswordStoreBackendMigrationDecoratorTest,
       InitialUPMMigrationDoesNotStartsIfSyncDisabled) {
  // Init backend.
  EXPECT_CALL(*built_in_backend(), InitBackend);
  EXPECT_CALL(*android_backend(), InitBackend);
  backend_migration_decorator()->InitBackend(
      /*affiliated_match_helper=*/nullptr,
      /*remote_form_changes_received=*/base::DoNothing(),
      /*sync_enabled_or_disabled_cb=*/base::DoNothing(),
      /*completion=*/base::DoNothing());

  // Set password sync to be active and have no auth errors.
  InitSyncService(/*is_password_sync_enabled=*/false);

  EXPECT_CALL(*built_in_backend(), GetAllLoginsAsync).Times(0);
  EXPECT_CALL(*android_backend(), GetAllLoginsAsync).Times(0);

  // Imitate successfully completing a sync cycle.
  FastForwardUntilNoTasksRemain();
  EXPECT_EQ(MigrationType::kNone, migration_in_progress_type());
}

}  // namespace password_manager
