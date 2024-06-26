// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNIN_OIDC_AUTHENTICATION_SIGNIN_INTERCEPTOR_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNIN_OIDC_AUTHENTICATION_SIGNIN_INTERCEPTOR_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/signin/token_managed_profile_creation_delegate.h"
#include "chrome/browser/signin/web_signin_interceptor.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"

namespace content {
class WebContents;
}

namespace policy {
class CloudPolicyClientRegistrationHelper;
}  // namespace policy

class OidcAuthenticationSigninInterceptorTest;

class Profile;
class ProfileAttributesEntry;

using OidcInterceptionCallback = base::OnceCallback<void()>;
using policy::CloudPolicyClient;

// Called after a valid OIDC authentication redirection is captured. The
// interceptor is responsible for starting registration process, collecting user
// consent, and creating/switching to a new managed profile if agreed.
class OidcAuthenticationSigninInterceptor : public WebSigninInterceptor,
                                            public KeyedService {
 public:
  enum class SigninInterceptionType {
    kProfileSwitch,
    kEnterprise,
  };

  OidcAuthenticationSigninInterceptor(
      Profile* profile,
      std::unique_ptr<WebSigninInterceptor::Delegate> delegate);
  ~OidcAuthenticationSigninInterceptor() override;

  OidcAuthenticationSigninInterceptor(
      const OidcAuthenticationSigninInterceptor&) = delete;
  OidcAuthenticationSigninInterceptor& operator=(
      const OidcAuthenticationSigninInterceptor&) = delete;

  // Intercept and kick off OIDC registration process if the tokens we received
  // are valid.
  virtual void MaybeInterceptOidcAuthentication(
      content::WebContents* intercepted_contents,
      ProfileManagementOicdTokens oidc_tokens,
      std::string issuer_id,
      std::string subject_id,
      OidcInterceptionCallback oidc_callback);

  // KeyedService:
  void Shutdown() override;

  void SetCloudPolicyClientForTesting(
      std::unique_ptr<CloudPolicyClient> client) {
    client_for_testing_ = std::move(client);
  }

 protected:
  virtual void CreateBrowserAfterSigninInterception();
  // Cancels any current signin interception and resets the interceptor to its
  // initial state.

 private:
  void Reset();

  // Try to send OIDC tokens to DM server for registration.
  void StartOidcRegistration();
  // Called when OIDC registration finishes, the client should be registered
  // (aka has a dm token) and various information should be included, most
  // importantly, if the 3P user identity is sync-ed to Google or not.
  void OnClientRegistered(std::unique_ptr<CloudPolicyClient> client,
                          std::string preset_profile_guid,
                          base::TimeTicks registration_start_time);

  // Called when user makes a decision on the profile creation dialog.
  void OnProfileCreationChoice(SigninInterceptionResult create);
  // Called when the new profile has been created.
  void OnNewSignedInProfileCreated(base::WeakPtr<Profile> new_profile);
  // Called when policy fetch response has been received, For Dasher-based
  // profiles, pulls gaia id from fetched policies and user email from DM server
  // response, and sets this AccountId as primary user of the new profile.
  void OnPolicyFetchCompleteInNewProfile(
      Profile* new_profile,
      base::TimeTicks policy_fetch_start_time,
      bool success);

  const raw_ptr<Profile, DanglingUntriaged> profile_;
  std::unique_ptr<Delegate> delegate_;
  std::unique_ptr<ManagedProfileCreator> profile_creator_;

  // Members below are related to the interception in progress.
  base::WeakPtr<content::WebContents> web_contents_;
  ProfileManagementOicdTokens oidc_tokens_;
  std::string dm_token_;
  std::string client_id_;
  std::string user_display_name_;
  std::string user_email_;
  // Unique id for the OIDC user, format:
  // "iss:<value of 'iss' field>,sub:<value of 'sub'field>"
  // For context, 'iss' is the ID of the OIDC issuer and 'sub' is the
  // unique-per-user subject ID within the issuer.
  std::string unique_user_identifier_;
  bool dasher_based_ = true;
  std::string preset_profile_id_;
  raw_ptr<const ProfileAttributesEntry> switch_to_entry_ = nullptr;
  SkColor profile_color_;
  // TODO(b/319479021): utilize the status variable to have better error
  // handling and in metrics.
  bool interception_in_progress_ = false;

  std::unique_ptr<policy::CloudPolicyClientRegistrationHelper>
      registration_helper_for_temporary_client_;

  // Used to retain the interception UI bubble until profile creation completes.
  std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>
      interception_bubble_handle_;

  std::unique_ptr<CloudPolicyClient> client_for_testing_ = nullptr;

  OidcInterceptionCallback oidc_callback_;

  base::WeakPtrFactory<OidcAuthenticationSigninInterceptor> weak_factory_{this};

  friend class OidcAuthenticationSigninInterceptorTest;
};

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNIN_OIDC_AUTHENTICATION_SIGNIN_INTERCEPTOR_H_