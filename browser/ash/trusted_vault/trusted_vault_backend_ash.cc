// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/trusted_vault/trusted_vault_backend_ash.h"

#include <utility>

#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account_manager_util.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/trusted_vault/trusted_vault_client.h"

namespace ash {

TrustedVaultBackendAsh::TrustedVaultBackendAsh(
    signin::IdentityManager* identity_manager,
    trusted_vault::TrustedVaultClient* trusted_vault_client)
    : identity_manager_(identity_manager),
      trusted_vault_client_(trusted_vault_client) {
  CHECK(identity_manager_);
  CHECK(trusted_vault_client_);
  trusted_vault_client_->AddObserver(this);
}

TrustedVaultBackendAsh::~TrustedVaultBackendAsh() {
  trusted_vault_client_->RemoveObserver(this);
}

void TrustedVaultBackendAsh::OnTrustedVaultKeysChanged() {
  for (auto& observer : observers_) {
    observer->OnTrustedVaultKeysChanged();
  }
}

void TrustedVaultBackendAsh::OnTrustedVaultRecoverabilityChanged() {
  for (auto& observer : observers_) {
    observer->OnTrustedVaultRecoverabilityChanged();
  }
}

void TrustedVaultBackendAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::TrustedVaultBackend> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void TrustedVaultBackendAsh::AddObserver(
    mojo::PendingRemote<crosapi::mojom::TrustedVaultBackendObserver> observer) {
  observers_.Add(std::move(observer));
}

void TrustedVaultBackendAsh::FetchKeys(
    crosapi::mojom::AccountKeyPtr account_key,
    FetchKeysCallback callback) {
  if (!ValidateAccountKeyIsPrimaryAccount(account_key)) {
    std::move(callback).Run(std::vector<std::vector<uint8_t>>());
    return;
  }
  trusted_vault_client_->FetchKeys(GetPrimaryAccountInfo(),
                                   std::move(callback));
}

void TrustedVaultBackendAsh::MarkLocalKeysAsStale(
    crosapi::mojom::AccountKeyPtr account_key,
    MarkLocalKeysAsStaleCallback callback) {
  if (!ValidateAccountKeyIsPrimaryAccount(account_key)) {
    std::move(callback).Run(false);
    return;
  }
  trusted_vault_client_->MarkLocalKeysAsStale(GetPrimaryAccountInfo(),
                                              std::move(callback));
}

void TrustedVaultBackendAsh::StoreKeys(
    crosapi::mojom::AccountKeyPtr account_key,
    const std::vector<std::vector<uint8_t>>& keys,
    int32_t last_key_version) {
  if (!ValidateAccountKeyIsPrimaryAccount(account_key)) {
    return;
  }
  trusted_vault_client_->StoreKeys(GetPrimaryAccountInfo().gaia, keys,
                                   last_key_version);
}

void TrustedVaultBackendAsh::GetIsRecoverabilityDegraded(
    crosapi::mojom::AccountKeyPtr account_key,
    GetIsRecoverabilityDegradedCallback callback) {
  if (!ValidateAccountKeyIsPrimaryAccount(account_key)) {
    std::move(callback).Run(false);
    return;
  }
  trusted_vault_client_->GetIsRecoverabilityDegraded(GetPrimaryAccountInfo(),
                                                     std::move(callback));
}

void TrustedVaultBackendAsh::AddTrustedRecoveryMethod(
    crosapi::mojom::AccountKeyPtr account_key,
    const std::vector<uint8_t>& public_key,
    int32_t method_type_hint,
    AddTrustedRecoveryMethodCallback callback) {
  if (!ValidateAccountKeyIsPrimaryAccount(account_key)) {
    std::move(callback).Run();
    return;
  }
  trusted_vault_client_->AddTrustedRecoveryMethod(GetPrimaryAccountInfo().gaia,
                                                  public_key, method_type_hint,
                                                  std::move(callback));
}

void TrustedVaultBackendAsh::ClearLocalDataForAccount(
    crosapi::mojom::AccountKeyPtr account_key) {
  if (!ValidateAccountKeyIsPrimaryAccount(account_key)) {
    return;
  }
  trusted_vault_client_->ClearLocalDataForAccount(GetPrimaryAccountInfo());
}

bool TrustedVaultBackendAsh::ValidateAccountKeyIsPrimaryAccount(
    const crosapi::mojom::AccountKeyPtr& mojo_account_key) const {
  const std::optional<account_manager::AccountKey> account_key =
      account_manager::FromMojoAccountKey(mojo_account_key);
  if (!account_key.has_value()) {
    return false;
  }

  if (account_key->account_type() != account_manager::AccountType::kGaia) {
    // ActiveDirectory accounts are not supported.
    return false;
  }

  return !account_key->id().empty() &&
         account_key->id() == GetPrimaryAccountInfo().gaia;
}

CoreAccountInfo TrustedVaultBackendAsh::GetPrimaryAccountInfo() const {
  return identity_manager_->GetPrimaryAccountInfo(
      signin::ConsentLevel::kSignin);
}

}  // namespace ash