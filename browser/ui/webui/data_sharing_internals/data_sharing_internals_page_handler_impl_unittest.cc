// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/data_sharing_internals/data_sharing_internals_page_handler_impl.h"

#include <set>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_sharing/public/data_sharing_network_loader.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/group_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;

namespace {

const char kGroup1Id[] = "g1";
const char kGroup1Name[] = "group1";
const char kMemberName[] = "John Doe";
const data_sharing::MemberRole kMemberRole = data_sharing::MemberRole::kOwner;

data_sharing::GroupData GetTestGroupData() {
  data_sharing::GroupData data;
  data.group_id = kGroup1Id;
  data.display_name = kGroup1Name;
  data_sharing::GroupMember member;
  member.display_name = kMemberName;
  member.role = kMemberRole;
  data.members.emplace_back(member);
  return data;
}

std::set<data_sharing::GroupData> GetTestGroupDataSet() {
  std::set<data_sharing::GroupData> result;
  result.emplace(GetTestGroupData());
  return result;
}

class MockPage : public data_sharing_internals::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<data_sharing_internals::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  mojo::Receiver<data_sharing_internals::mojom::Page> receiver_{this};
};

class MockDataSharingService : public data_sharing::DataSharingService {
 public:
  MockDataSharingService() = default;
  ~MockDataSharingService() override = default;

  // Disallow copy/assign.
  MockDataSharingService(const MockDataSharingService&) = delete;
  MockDataSharingService& operator=(const MockDataSharingService&) = delete;

  // DataSharingNetworkLoader Impl.
  MOCK_METHOD(bool, IsEmptyService, (), (override));
  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
  MOCK_METHOD(data_sharing::DataSharingNetworkLoader*,
              GetDataSharingNetworkLoader,
              (),
              (override));
  MOCK_METHOD(base::WeakPtr<syncer::ModelTypeControllerDelegate>,
              GetCollaborationGroupControllerDelegate,
              (),
              (override));
  MOCK_METHOD(void,
              ReadAllGroups,
              (base::OnceCallback<void(const GroupsDataSetOrFailureOutcome&)>),
              (override));
  MOCK_METHOD(void,
              ReadGroup,
              (const std::string&,
               base::OnceCallback<void(const GroupDataOrFailureOutcome&)>),
              (override));
  MOCK_METHOD(void,
              CreateGroup,
              (const std::string&,
               base::OnceCallback<void(const GroupDataOrFailureOutcome&)>),
              (override));
  MOCK_METHOD(void,
              DeleteGroup,
              (const std::string&,
               base::OnceCallback<void(PeopleGroupActionOutcome)>),
              (override));
  MOCK_METHOD(void,
              InviteMember,
              (const std::string&,
               const std::string&,
               base::OnceCallback<void(PeopleGroupActionOutcome)>),
              (override));
  MOCK_METHOD(void,
              RemoveMember,
              (const std::string&,
               const std::string&,
               base::OnceCallback<void(PeopleGroupActionOutcome)>),
              (override));
  MOCK_METHOD(bool,
              ShouldInterceptNavigationForShareURL,
              (const GURL&),
              (override));
  MOCK_METHOD(void,
              HandleShareURLNavigationIntercepted,
              (const GURL&),
              (override));
};
}  // namespace

class DataSharingInternalsPageHandlerImplTest : public testing::Test {
 public:
  DataSharingInternalsPageHandlerImplTest()
      : handler_(std::make_unique<DataSharingInternalsPageHandlerImpl>(
            mojo::PendingReceiver<data_sharing_internals::mojom::PageHandler>(),
            mock_client_.BindAndGetRemote(),
            &data_sharing_service_)) {}
  ~DataSharingInternalsPageHandlerImplTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  MockDataSharingService data_sharing_service_;
  testing::NiceMock<MockPage> mock_client_;
  std::unique_ptr<DataSharingInternalsPageHandlerImpl> handler_;
};

TEST_F(DataSharingInternalsPageHandlerImplTest, UseEmptyService) {
  EXPECT_CALL(data_sharing_service_, IsEmptyService())
      .WillRepeatedly(Return(true));
  base::RunLoop run_loop;
  handler_->IsEmptyService(base::BindOnce(
      [](base::RunLoop* run_loop, bool success) {
        ASSERT_TRUE(success);
        run_loop->Quit();
      },
      &run_loop));
  run_loop.Run();
}

TEST_F(DataSharingInternalsPageHandlerImplTest, UseNonEmptyService) {
  EXPECT_CALL(data_sharing_service_, IsEmptyService())
      .WillRepeatedly(Return(false));
  base::RunLoop run_loop;
  handler_->IsEmptyService(base::BindOnce(
      [](base::RunLoop* run_loop, bool success) {
        ASSERT_FALSE(success);
        run_loop->Quit();
      },
      &run_loop));
  run_loop.Run();
}

TEST_F(DataSharingInternalsPageHandlerImplTest, GetAllGroupsWithError) {
  EXPECT_CALL(data_sharing_service_, ReadAllGroups)
      .WillOnce([](base::OnceCallback<void(
                       const data_sharing::DataSharingService::
                           GroupsDataSetOrFailureOutcome&)> callback) {
        std::move(callback).Run(
            base::unexpected(data_sharing::DataSharingService::
                                 PeopleGroupActionFailure::kTransientFailure));
      });
  base::RunLoop run_loop;
  handler_->GetAllGroups(base::BindOnce(
      [](base::RunLoop* run_loop, bool success,
         std::vector<data_sharing_internals::mojom::GroupDataPtr> result) {
        ASSERT_FALSE(success);
        run_loop->Quit();
      },
      &run_loop));
  run_loop.Run();
}

TEST_F(DataSharingInternalsPageHandlerImplTest, GetAllGroups) {
  EXPECT_CALL(data_sharing_service_, ReadAllGroups)
      .WillOnce([](base::OnceCallback<void(
                       const data_sharing::DataSharingService::
                           GroupsDataSetOrFailureOutcome&)> callback) {
        std::move(callback).Run(base::ok(GetTestGroupDataSet()));
      });
  base::RunLoop run_loop;
  handler_->GetAllGroups(base::BindOnce(
      [](base::RunLoop* run_loop, bool success,
         std::vector<data_sharing_internals::mojom::GroupDataPtr> result) {
        ASSERT_TRUE(success);
        ASSERT_EQ(result.size(), 1u);
        ASSERT_EQ(result[0]->group_id, kGroup1Id);
        ASSERT_EQ(result[0]->name, kGroup1Name);
        ASSERT_EQ(result[0]->members.size(), 1u);
        ASSERT_EQ(result[0]->members[0]->display_name, kMemberName);
        ASSERT_EQ(result[0]->members[0]->role,
                  data_sharing_internals::mojom::RoleType::OWNER);
        run_loop->Quit();
      },
      &run_loop));
  run_loop.Run();
}
