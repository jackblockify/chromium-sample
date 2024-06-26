// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_HOTLOG2_DATA_AGGREGATOR_SERVICE_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_HOTLOG2_DATA_AGGREGATOR_SERVICE_H_

#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/command_source.h"
#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/log_source.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_observer.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/service_adaptor.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/meet_devices_data_aggregator.mojom-shared.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/meet_devices_data_aggregator.mojom.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/meet_devices_info.mojom.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/meet_devices_logger.mojom.h"
#include "chromeos/services/chromebox_for_meetings/public/proto/transport_payload.pb.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::cfm {

// This service manages the aggregation of data from one or more
// DataSources, as well as "processing" the data, which includes
// uploading the data to an external server (eg for cloud logging),
// and adding watchdogs to any data source for on-demand monitoring.
// This is also the class that exposes its API over hotline for
// external clients to communicate with.
class DataAggregatorService : public CfmObserver,
                              public chromeos::cfm::ServiceAdaptor::Delegate,
                              public mojom::DataAggregator {
 public:
  DataAggregatorService();
  ~DataAggregatorService() override;
  DataAggregatorService(const DataAggregatorService&) = delete;
  DataAggregatorService& operator=(const DataAggregatorService&) = delete;

  // Manage singleton instance.
  static void Initialize();
  static void InitializeForTesting(
      DataAggregatorService* data_aggregator_service);
  static void Shutdown();
  static DataAggregatorService* Get();
  static bool IsInitialized();

 protected:
  // CfmObserver:
  bool ServiceRequestReceived(const std::string& interface_name) override;

  // chromeos::cfm::ServiceAdaptor::Delegate:
  void OnAdaptorDisconnect() override;
  void OnBindService(mojo::ScopedMessagePipeHandle receiver_pipe) override;

  // mojom::DataAggregator implementation
  void GetDataSourceNames(GetDataSourceNamesCallback callback) override;
  void AddDataSource(const std::string& source_name,
                     mojo::PendingRemote<mojom::DataSource> new_data_source,
                     AddDataSourceCallback callback) override;
  void AddWatchDog(const std::string& source_name,
                   mojom::DataFilterPtr filter,
                   mojo::PendingRemote<mojom::DataWatchDog> watch_dog,
                   AddWatchDogCallback callback) override;

  // Disconnect handler for |mojom::DataAggregator|
  virtual void OnMojoDisconnect();

  // Will be overridden by test object for more controlled test environment
  virtual void InitializeLocalSources();

  // Maps DataSource names to their remotes, for access convenience
  std::map<std::string, mojo::Remote<mojom::DataSource>> data_source_map_;

 private:
  void AddLocalCommandSource(const std::string& command,
                             const base::TimeDelta& poll_freq);
  void OnLocalCommandDisconnect(const std::string& command,
                                const base::TimeDelta& poll_freq);
  void AddLocalLogSource(const std::string& filepath);
  void OnLocalLogDisconnect(const std::string& filepath);
  void InitializeUploadEndpoint(size_t num_tries);
  void OnRequestBindUploadService(const std::string& interface_name,
                                  size_t num_tries,
                                  bool success);
  void InitializeDeviceInfoEndpoint(size_t num_tries);
  void OnRequestBindDeviceInfoService(const std::string& interface_name,
                                      size_t num_tries,
                                      bool success);
  void RequestDeviceId();
  void StoreDeviceId(chromeos::cfm::mojom::PolicyInfoPtr policy_info);
  void StartFetchTimer();
  void FetchFromAllSourcesAndEnqueue();
  void EnqueueData(const std::string& source_name,
                   const std::vector<std::string>& serialized_entries);
  void WrapEntriesInTransportPayload(
      const std::string& source_name,
      const std::vector<std::string>& serialized_entries,
      proto::TransportPayload* transport_payload);
  void HandleEnqueueResponse(const std::string& source_name,
                             chromeos::cfm::mojom::LoggerStatusPtr status);

  chromeos::cfm::ServiceAdaptor service_adaptor_;
  mojo::ReceiverSet<mojom::DataAggregator> receivers_;

  base::RepeatingTimer fetch_timer_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Worker thread for locally created DataSources
  scoped_refptr<base::SequencedTaskRunner> local_task_runner_;

  // Remote endpoint for CfmLoggerService
  mojo::Remote<chromeos::cfm::mojom::MeetDevicesLogger> uploader_remote_;

  // Remote endpoint for CfmDeviceInfoService
  mojo::Remote<chromeos::cfm::mojom::MeetDevicesInfo> device_info_remote_;

  // Unique device ID for the CfM that is permanent across provisioning.
  std::string device_id_;

  // Must be the last class member.
  base::WeakPtrFactory<DataAggregatorService> weak_ptr_factory_{this};
};

}  // namespace ash::cfm

#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_HOTLOG2_DATA_AGGREGATOR_SERVICE_H_
