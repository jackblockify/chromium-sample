// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/most_relevant_tab_resumption/most_relevant_tab_resumption_page_handler.h"

#include <vector>

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "chrome/browser/visited_url_ranking/visited_url_ranking_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/mojom/history_types.mojom.h"
#include "components/search/ntp_features.h"
#include "components/visited_url_ranking/public/test_support.h"
#include "components/visited_url_ranking/public/testing/mock_visited_url_ranking_service.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using visited_url_ranking::Fetcher;
using visited_url_ranking::FetchOptions;
using visited_url_ranking::ResultStatus;
using visited_url_ranking::URLVisit;
using visited_url_ranking::URLVisitAggregate;
using visited_url_ranking::VisitedURLRankingService;
using visited_url_ranking::VisitedURLRankingServiceFactory;

namespace {

class MostRelevantTabResumptionPageHandlerTest
    : public BrowserWithTestWindowTest {
 public:
  MostRelevantTabResumptionPageHandlerTest() = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));
    handler_ = std::make_unique<MostRelevantTabResumptionPageHandler>(
        mojo::PendingReceiver<
            ntp::most_relevant_tab_resumption::mojom::PageHandler>(),
        web_contents_.get());
  }

  std::vector<history::mojom::TabPtr> RunGetTabs() {
    std::vector<history::mojom::TabPtr> tabs_mojom;
    base::RunLoop wait_loop;
    handler_->GetTabs(base::BindOnce(
        [](base::OnceClosure stop_waiting,
           std::vector<history::mojom::TabPtr>* tabs,
           std::vector<history::mojom::TabPtr> tabs_arg) {
          *tabs = std::move(tabs_arg);
          std::move(stop_waiting).Run();
        },
        wait_loop.QuitClosure(), &tabs_mojom));
    wait_loop.Run();
    return tabs_mojom;
  }

  void TearDown() override {
    handler_.reset();
    web_contents_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

 private:
  // BrowserWithTestWindowTest:
  TestingProfile::TestingFactories GetTestingFactories() override {
    return {
        {VisitedURLRankingServiceFactory::GetInstance(),
         base::BindRepeating([](content::BrowserContext* context)
                                 -> std::unique_ptr<KeyedService> {
           return std::make_unique<
               visited_url_ranking::MockVisitedURLRankingService>();
         })},
    };
  }

  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<MostRelevantTabResumptionPageHandler> handler_;
};

}  // namespace

using testing::_;

TEST_F(MostRelevantTabResumptionPageHandlerTest, GetFakeTabs) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {
          {ntp_features::kNtpMostRelevantTabResumptionModule,
           {{ntp_features::kNtpMostRelevantTabResumptionModuleDataParam,
             "Fake Data"}}},
      },
      {});

  auto tabs_mojom = RunGetTabs();
  ASSERT_EQ(3u, tabs_mojom.size());
  for (const auto& tab_mojom : tabs_mojom) {
    ASSERT_EQ("Test Session", tab_mojom->session_name);
    ASSERT_EQ("5 mins ago", tab_mojom->relative_time_text);
    ASSERT_EQ(GURL("https://www.google.com"), tab_mojom->url);
  }
}

TEST_F(MostRelevantTabResumptionPageHandlerTest, GetTabs) {
  visited_url_ranking::MockVisitedURLRankingService*
      mock_visited_url_ranking_service =
          static_cast<visited_url_ranking::MockVisitedURLRankingService*>(
              VisitedURLRankingServiceFactory::GetForProfile(profile()));

  EXPECT_CALL(*mock_visited_url_ranking_service, FetchURLVisitAggregates(_, _))
      .Times(1)
      .WillOnce(testing::Invoke(
          [](const FetchOptions& options,
             VisitedURLRankingService::GetURLVisitAggregatesCallback callback) {
            std::vector<URLVisitAggregate> url_visit_aggregates = {};
            url_visit_aggregates.emplace_back(
                visited_url_ranking::CreateSampleURLVisitAggregate(
                    GURL(visited_url_ranking::kSampleSearchUrl), 1.0f,
                    base::Time::Now(), {Fetcher::kSession}));
            url_visit_aggregates.emplace_back(
                visited_url_ranking::CreateSampleURLVisitAggregate(
                    GURL(visited_url_ranking::kSampleSearchUrl), 1.0f,
                    base::Time::Now(), {Fetcher::kHistory}));

            std::move(callback).Run(ResultStatus::kSuccess,
                                    std::move(url_visit_aggregates));
          }));

  auto tabs_mojom = RunGetTabs();
  ASSERT_EQ(2u, tabs_mojom.size());
  for (const auto& tab_mojom : tabs_mojom) {
    ASSERT_EQ(history::mojom::DeviceType::kUnknown, tab_mojom->device_type);
    ASSERT_EQ("sample_title", tab_mojom->title);
    ASSERT_EQ(GURL(visited_url_ranking::kSampleSearchUrl), tab_mojom->url);
  }
}
