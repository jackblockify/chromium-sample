// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_TRUSTED_VAULT_TRUSTED_VAULT_BACKEND_ASH_H_
#define CHROME_BROWSER_ASH_TRUSTED_VAULT_TRUSTED_VAULT_BACKEND_ASH_H_

#include <cstdint>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "chromeos/crosapi/mojom/trusted_vault.mojom.h"
#include "components/trusted_vault/trusted_vault_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

struct CoreAccountInfo;

namespace signin {
class IdentityManager;
}

namespace ash {

class TrustedVaultBackendAsh
    : public crosapi::mojom::TrustedVaultBackend,
      public trusted_vault::TrustedVaultClient::Observer {
 public:
  // `identity_manager` and `trusted_vault_client` must not be null.
  TrustedVaultBackendAsh(
      signin::IdentityManager* identity_manager,
      trusted_vault::TrustedVaultClient* trusted_vault_client);
  TrustedVaultBackendAsh(const TrustedVaultBackendAsh&) = delete;
  TrustedVaultBackendAsh& operator=(const TrustedVaultBackendAsh&) = delete;
  ~TrustedVaultBackendAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::TrustedVaultBackend> receiver);

  // trusted_vault::TrustedVaultClient::Observer implementation.
  void OnTrustedVaultKeysChanged() override;
  void OnTrustedVaultRecoverabilityChanged() override;

  // crosapi::mojom::TrustedVaultBackend implementation.
  void AddObserver(
      mojo::PendingRemote<crosapi::mojom::TrustedVaultBackendObserver> observer)
      override;
  void FetchKeys(crosapi::mojom::AccountKeyPtr account_key,
                 FetchKeysCallback callback) override;
  void MarkLocalKeysAsStale(crosapi::mojom::AccountKeyPtr account_key,
                            MarkLocalKeysAsStaleCallback callback) override;
  void StoreKeys(crosapi::mojom::AccountKeyPtr account_key,
                 const std::vector<std::vector<uint8_t>>& keys,
                 int32_t last_key_version) override;
  void GetIsRecoverabilityDegraded(
      crosapi::mojom::AccountKeyPtr account_key,
      GetIsRecoverabilityDegradedCallback callback) override;
  void AddTrustedRecoveryMethod(
      crosapi::mojom::AccountKeyPtr account_key,
      const std::vector<uint8_t>& public_key,
      int32_t method_type_hint,
      AddTrustedRecoveryMethodCallback callback) override;
  void ClearLocalDataForAccount(
      crosapi::mojom::AccountKeyPtr account_key) override;

 private:
  bool ValidateAccountKeyIsPrimaryAccount(
      const crosapi::mojom::AccountKeyPtr& account_key) const;
  CoreAccountInfo GetPrimaryAccountInfo() const;

  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<trusted_vault::TrustedVaultClient> trusted_vault_client_;

  // Don't add new members below this. `receivers_` and `observers_` should be
  // destroyed as soon as `this` (or prior that) is getting destroyed so that we
  // don't deal with message handling on a partially destroyed object.
  mojo::ReceiverSet<crosapi::mojom::TrustedVaultBackend> receivers_;
  mojo::RemoteSet<crosapi::mojom::TrustedVaultBackendObserver> observers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_TRUSTED_VAULT_TRUSTED_VAULT_BACKEND_ASH_H_
