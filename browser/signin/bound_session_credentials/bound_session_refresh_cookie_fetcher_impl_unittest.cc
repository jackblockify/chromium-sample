// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/containers/span.h"
#include "base/json/json_reader.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params_util.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_test_cookie_manager.h"
#include "chrome/browser/signin/bound_session_credentials/rotation_debug_info.pb.h"
#include "chrome/browser/signin/bound_session_credentials/session_binding_helper.h"
#include "components/signin/public/base/session_binding_test_utils.h"
#include "components/signin/public/base/session_binding_utils.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/unexportable_key.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using RefreshTestFuture =
    base::test::TestFuture<BoundSessionRefreshCookieFetcher::Result>;
using Result = BoundSessionRefreshCookieFetcher::Result;
using base::test::RunOnceCallback;
using bound_session_credentials::RotationDebugInfo;
using testing::_;
using testing::ElementsAre;
using testing::FieldsAre;
using unexportable_keys::BackgroundTaskPriority;
using unexportable_keys::ServiceErrorOr;
using unexportable_keys::UnexportableKeyId;
using unexportable_keys::UnexportableKeyService;

constexpr char kSessionId[] = "session_id";
constexpr char kChallenge[] = "aGVsbG8_d29ybGQ";
constexpr net::Error kConnectionNetError = net::ERR_UNEXPECTED;

MATCHER_P3(JwtHasExpectedFields, session_id, challenge, destination_url, "") {
  std::string_view jwt = arg;
  std::optional<base::Value::Dict> payload_dict =
      signin::ExtractPayloadFromJwt(jwt);
  if (!payload_dict) {
    *result_listener << "Couldn't parse payload from JWT: " << jwt;
    return false;
  }

  *result_listener << " with payload " << payload_dict->DebugString();
  return testing::ExplainMatchResult(
      base::test::DictionaryHasValues(base::Value::Dict()
                                          .Set("sub", session_id)
                                          .Set("jti", challenge)
                                          .Set("aud", destination_url.spec())),
      *payload_dict, result_listener);
}

class MockSessionBindingHelper : public SessionBindingHelper {
 public:
  explicit MockSessionBindingHelper(
      unexportable_keys::UnexportableKeyService& unexportable_key_service)
      : SessionBindingHelper(unexportable_key_service,
                             base::span<uint8_t>(),
                             std::string()) {}

  ~MockSessionBindingHelper() override = default;

  MOCK_METHOD(void, MaybeLoadBindingKey, (), (override));
  MOCK_METHOD(
      void,
      GenerateBindingKeyAssertion,
      (std::string_view challenge,
       const GURL& destination_url,
       base::OnceCallback<void(base::expected<std::string, Error>)> callback),
      (override));
};

UnexportableKeyId GenerateNewKey(
    UnexportableKeyService& unexportable_key_service) {
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
  unexportable_key_service.GenerateSigningKeySlowlyAsync(
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>(
          {crypto::SignatureVerifier::ECDSA_SHA256}),
      BackgroundTaskPriority::kUserBlocking, generate_future.GetCallback());
  ServiceErrorOr<UnexportableKeyId> key_id = generate_future.Get();
  CHECK(key_id.has_value());
  return *key_id;
}

std::string CreateChallengeHeaderValue(
    const std::string& challenge,
    const std::string& session_id = kSessionId) {
  return base::StringPrintf("session_id=%s; challenge=%s", session_id.c_str(),
                            challenge.c_str());
}
}  // namespace

class BoundSessionRefreshCookieFetcherImplTest : public ::testing::Test {
 public:
  BoundSessionRefreshCookieFetcherImplTest()
      : unexportable_key_service_(unexportable_key_task_manager_) {
    binding_key_id_ = GenerateNewKey(unexportable_key_service_);
    session_binding_helper_ = std::make_unique<SessionBindingHelper>(
        unexportable_key_service_,
        *unexportable_key_service_.GetWrappedKey(binding_key_id_), kSessionId);
    fetcher_ = std::make_unique<BoundSessionRefreshCookieFetcherImpl>(
        test_url_loader_factory_.GetSafeWeakWrapper(), *session_binding_helper_,
        kSessionId, /*refresh_url=*/GURL(), kGaiaUrl,
        base::flat_set<std::string>{k1PSIDTSCookieName, k3PSIDTSCookieName},
        /*is_off_the_record_profile=*/false,
        bound_session_credentials::RotationDebugInfo());
    UpdateCookieList();
  }

 protected:
  const GURL kGaiaUrl = GURL("https://google.com/");
  const std::string k1PSIDTSCookieName = "__Secure-1PSIDTS";
  const std::string k3PSIDTSCookieName = "__Secure-3PSIDTS";

  void UpdateCookieList(
      const base::flat_set<std::string>& excluded_cookies = {}) {
    const std::string kCookiesNames[4] = {
        "__Secure-cookie1", k1PSIDTSCookieName, "__Secure-cookie2",
        k3PSIDTSCookieName};
    cookies_.clear();
    for (const auto& cookie_name : kCookiesNames) {
      if (excluded_cookies.contains(cookie_name)) {
        continue;
      }
      cookies_.emplace_back(
          BoundSessionTestCookieManager::CreateCookie(kGaiaUrl, cookie_name));
    }
  }

  std::vector<network::mojom::CookieOrLineWithAccessResultPtr>
  CreateReportedCookies(const net::CookieList& cookie_list) {
    std::vector<network::mojom::CookieOrLineWithAccessResultPtr>
        reported_cookies;
    for (auto cookie : cookie_list) {
      network::mojom::CookieOrLinePtr cookie_or_line =
          network::mojom::CookieOrLine::NewCookie(cookie);
      reported_cookies.push_back(
          network::mojom::CookieOrLineWithAccessResult::New(
              std::move(cookie_or_line), net::CookieAccessResult()));
    }
    return reported_cookies;
  }

  void SimulateChallengeRequired(const std::string& challenge_header) {
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
    network::TestURLLoaderFactory::PendingRequest* pending_request =
        test_url_loader_factory_.GetPendingRequest(0);
    network::URLLoaderCompletionStatus ok_completion_status(net::OK);
    auto response = network::CreateURLResponseHead(net::HTTP_UNAUTHORIZED);

    response->headers->AddHeader("Sec-Session-Google-Challenge",
                                 challenge_header);
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        pending_request->request.url, ok_completion_status, std::move(response),
        ""));
  }

  void SimulateOnCookiesAccessed(
      network::mojom::CookieAccessDetails::Type access_type) {
    std::vector<network::mojom::CookieAccessDetailsPtr> cookie_access_details;
    cookie_access_details.emplace_back(network::mojom::CookieAccessDetails::New(
        access_type, kGaiaUrl, url::Origin(), net::SiteForCookies(),
        CreateReportedCookies(cookies_), /*devtools_request_id=*/std::nullopt,
        /*count=*/1,
        /*is_ad_tagged=*/false, net::CookieSettingOverrides(),
        /*source_location=*/nullptr));
    fetcher_->OnCookiesAccessed(std::move(cookie_access_details));
  }

  bool reported_cookies_notified() {
    return fetcher_->reported_cookies_notified_;
  }

  bool expected_cookies_set() { return fetcher_->expected_cookies_set_; }

  void VerifyMetricsRecorded(
      BoundSessionRefreshCookieFetcher::Result expected_result,
      size_t expect_assertion_was_generated_count) {
    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    "Signin.BoundSessionCredentials.CookieRotationResult"),
                ElementsAre(base::Bucket(expected_result, /*count=*/1)));
    histogram_tester_.ExpectTotalCount(
        "Signin.BoundSessionCredentials.CookieRotationTotalDuration", 1);
    histogram_tester_.ExpectTotalCount(
        "Signin.BoundSessionCredentials."
        "CookieRotationGenerateAssertionDuration",
        expect_assertion_was_generated_count);

    std::vector<base::Bucket> expected_net_error_buckets;
    if (expected_result == Result::kConnectionError) {
      expected_net_error_buckets.emplace_back(-kConnectionNetError,
                                              /*count=*/1);
    }
    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    "Signin.BoundSessionCredentials.CookieRotationNetError"),
                testing::ElementsAreArray(expected_net_error_buckets));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  crypto::ScopedMockUnexportableKeyProvider scoped_key_provider_;
  unexportable_keys::UnexportableKeyTaskManager unexportable_key_task_manager_{
      crypto::UnexportableKeyProvider::Config()};
  unexportable_keys::UnexportableKeyServiceImpl unexportable_key_service_;
  UnexportableKeyId binding_key_id_;
  std::unique_ptr<SessionBindingHelper> session_binding_helper_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<BoundSessionRefreshCookieFetcherImpl> fetcher_;
  net::CookieList cookies_;
  base::HistogramTester histogram_tester_;
};

TEST_F(BoundSessionRefreshCookieFetcherImplTest, SuccessExpectedCookieSet) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_.total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url,
            "https://accounts.google.com/RotateBoundCookies");
  EXPECT_EQ(pending_request->request.method, "GET");
  EXPECT_EQ(pending_request->request.credentials_mode,
            network::mojom::CredentialsMode::kInclude);

  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(reported_cookies_notified());
  EXPECT_TRUE(expected_cookies_set());

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), Result::kSuccess);
  EXPECT_FALSE(fetcher_->IsChallengeReceived());
  VerifyMetricsRecorded(Result::kSuccess,
                        /*expect_assertion_was_generated_count=*/0);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       SuccessWithChallengeNonEmptyRefreshUrl) {
  const GURL refresh_url("https://security.google.com/CheckBinding");
  fetcher_ = std::make_unique<BoundSessionRefreshCookieFetcherImpl>(
      test_url_loader_factory_.GetSafeWeakWrapper(), *session_binding_helper_,
      kSessionId, refresh_url, kGaiaUrl,
      base::flat_set<std::string>{k1PSIDTSCookieName, k3PSIDTSCookieName},
      /*is_off_the_record_profile=*/false,
      bound_session_credentials::RotationDebugInfo());
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  SimulateChallengeRequired(CreateChallengeHeaderValue(kChallenge));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url, refresh_url);
  auto headers = pending_request->request.headers;
  std::string assertion;
  EXPECT_TRUE(headers.GetHeader("Sec-Session-Google-Response", &assertion));

  EXPECT_TRUE(signin::VerifyJwtSignature(
      assertion, *unexportable_key_service_.GetAlgorithm(binding_key_id_),
      *unexportable_key_service_.GetSubjectPublicKeyInfo(binding_key_id_)));
  EXPECT_THAT(assertion,
              JwtHasExpectedFields(kSessionId, kChallenge, refresh_url));

  // Set required cookies and complete the request.
  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), Result::kSuccess);
  VerifyMetricsRecorded(Result::kSuccess,
                        /*expect_assertion_was_generated_count=*/1);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       SuccessCookiesReportedDelayed) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_.total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");
  EXPECT_FALSE(future.IsReady());
  EXPECT_FALSE(reported_cookies_notified());

  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(reported_cookies_notified());
  EXPECT_TRUE(expected_cookies_set());

  EXPECT_EQ(future.Get(), Result::kSuccess);
  VerifyMetricsRecorded(Result::kSuccess,
                        /*expect_assertion_was_generated_count=*/0);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       ResultNotReportedOnCookieRead) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_.total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");
  EXPECT_FALSE(future.IsReady());
  EXPECT_FALSE(reported_cookies_notified());

  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kRead);
  EXPECT_FALSE(future.IsReady());
  EXPECT_FALSE(reported_cookies_notified());

  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  EXPECT_EQ(future.Get(), Result::kSuccess);
  VerifyMetricsRecorded(Result::kSuccess,
                        /*expect_assertion_was_generated_count=*/0);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, CookiesNotReported) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_.total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");
  EXPECT_FALSE(future.IsReady());
  EXPECT_FALSE(reported_cookies_notified());

  task_environment_.FastForwardBy(base::Milliseconds(100));
  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(reported_cookies_notified());
  EXPECT_EQ(future.Get(), Result::kServerUnexepectedResponse);
  VerifyMetricsRecorded(Result::kServerUnexepectedResponse,
                        /*expect_assertion_was_generated_count=*/0);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       CookiesReportedExpectedCookieNotSet) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_.total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);

  UpdateCookieList(
      /*excluded_cookies=*/{k1PSIDTSCookieName, k3PSIDTSCookieName});
  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(reported_cookies_notified());
  EXPECT_FALSE(expected_cookies_set());

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), Result::kServerUnexepectedResponse);
  VerifyMetricsRecorded(Result::kServerUnexepectedResponse,
                        /*expect_assertion_was_generated_count=*/0);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       CookiesReportedNotAllExpectedCookiesSet) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_.total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);

  // Remove one of the expected cookies
  UpdateCookieList(/*excluded_cookies=*/{k3PSIDTSCookieName});
  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(reported_cookies_notified());
  EXPECT_FALSE(expected_cookies_set());

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), Result::kServerUnexepectedResponse);
  VerifyMetricsRecorded(Result::kServerUnexepectedResponse,
                        /*expect_assertion_was_generated_count=*/0);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, FailureNetError) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_.total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);

  network::URLLoaderCompletionStatus status(kConnectionNetError);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url, status,
      network::mojom::URLResponseHead::New(), std::string());

  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(reported_cookies_notified());
  BoundSessionRefreshCookieFetcher::Result result = future.Get<0>();
  EXPECT_EQ(result, Result::kConnectionError);
  VerifyMetricsRecorded(Result::kConnectionError,
                        /*expect_assertion_was_generated_count=*/0);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, FailureHttpError) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_.total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "", net::HTTP_UNAUTHORIZED);

  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(reported_cookies_notified());
  BoundSessionRefreshCookieFetcher::Result result = future.Get();
  EXPECT_EQ(result, Result::kServerPersistentError);
  VerifyMetricsRecorded(Result::kServerPersistentError,
                        /*expect_assertion_was_generated_count=*/0);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, ChallengeRequired) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());
  EXPECT_FALSE(fetcher_->IsChallengeReceived());

  SimulateChallengeRequired(CreateChallengeHeaderValue(kChallenge));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);
  auto headers = pending_request->request.headers;
  std::string assertion;
  EXPECT_TRUE(headers.GetHeader("Sec-Session-Google-Response", &assertion));

  EXPECT_TRUE(signin::VerifyJwtSignature(
      assertion, *unexportable_key_service_.GetAlgorithm(binding_key_id_),
      *unexportable_key_service_.GetSubjectPublicKeyInfo(binding_key_id_)));
  EXPECT_THAT(assertion,
              JwtHasExpectedFields(
                  kSessionId, kChallenge,
                  GURL("https://accounts.google.com/RotateBoundCookies")));

  // Set required cookies and complete the request.
  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), Result::kSuccess);
  EXPECT_TRUE(fetcher_->IsChallengeReceived());
  VerifyMetricsRecorded(Result::kSuccess,
                        /*expect_assertion_was_generated_count=*/1);
  histogram_tester_.ExpectUniqueSample(
      "Signin.BoundSessionCredentials.CookieRotationSessionIdsMatch", true,
      /*expected_bucket_count=*/1);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       ChallengeRequiredNonUTF8Characters) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  SimulateChallengeRequired(CreateChallengeHeaderValue("\xF0\x8F\xBF\xBE"));
  EXPECT_EQ(future.Get(), Result::kChallengeRequiredUnexpectedFormat);
  VerifyMetricsRecorded(Result::kChallengeRequiredUnexpectedFormat,
                        /*expect_assertion_was_generated_count=*/0);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       BadChallengeHeaderFormatEmpty) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());
  SimulateChallengeRequired("");
  EXPECT_EQ(future.Get(), Result::kChallengeRequiredUnexpectedFormat);
  VerifyMetricsRecorded(Result::kChallengeRequiredUnexpectedFormat,
                        /*expect_assertion_was_generated_count=*/0);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       BadChallengeHeaderFormatChallengeMissing) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());
  SimulateChallengeRequired("session_id=12345;");
  EXPECT_EQ(future.Get(), Result::kChallengeRequiredUnexpectedFormat);
  VerifyMetricsRecorded(Result::kChallengeRequiredUnexpectedFormat,
                        /*expect_assertion_was_generated_count=*/0);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       BadChallengeHeaderSessionIdsDontMatch) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());
  // Session IDs mismatch doesn't cause failures yet but gets reported to a
  // histogram below.
  // TODO(http://b/341261442): this test should expect a failure once the
  // session ID match is enforced.
  SimulateChallengeRequired(
      CreateChallengeHeaderValue(/*challenge=*/"test", /*session_id=*/"12345"));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(future.IsReady());

  // Set required cookies and complete the request.
  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      test_url_loader_factory_.GetPendingRequest(0)->request.url.spec(), "");

  EXPECT_EQ(future.Get(), Result::kSuccess);
  EXPECT_TRUE(fetcher_->IsChallengeReceived());

  histogram_tester_.ExpectUniqueSample(
      "Signin.BoundSessionCredentials.CookieRotationSessionIdsMatch", false,
      /*expected_bucket_count=*/1);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       BadChallengeHeaderFormatChallengeFieldEmpty) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());
  SimulateChallengeRequired(CreateChallengeHeaderValue(""));
  EXPECT_EQ(future.Get(), Result::kChallengeRequiredUnexpectedFormat);
  VerifyMetricsRecorded(Result::kChallengeRequiredUnexpectedFormat,
                        /*expect_assertion_was_generated_count=*/0);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       AssertionRequestsLimitExceeded) {
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  size_t assertion_requests = 0;
  const size_t max_assertion_requests_allowed = 5;
  do {
    SimulateChallengeRequired(CreateChallengeHeaderValue(kChallenge));
    task_environment_.RunUntilIdle();
    assertion_requests++;
    ASSERT_EQ(future.IsReady(),
              assertion_requests > max_assertion_requests_allowed);
  } while (!future.IsReady());
  EXPECT_EQ(future.Get(), Result::kChallengeRequiredLimitExceeded);
  VerifyMetricsRecorded(
      Result::kChallengeRequiredLimitExceeded,
      /*expect_assertion_was_generated_count=*/assertion_requests - 1);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, SignChallengeFailed) {
  // Use fake wrapped key.
  std::vector<uint8_t> wrapped_key{1, 2, 3, 4, 5};
  fetcher_.reset();
  session_binding_helper_ = std::make_unique<SessionBindingHelper>(
      unexportable_key_service_, wrapped_key, kSessionId);
  fetcher_ = std::make_unique<BoundSessionRefreshCookieFetcherImpl>(
      test_url_loader_factory_.GetSafeWeakWrapper(), *session_binding_helper_,
      kSessionId, /*refresh_url=*/GURL(), kGaiaUrl,
      base::flat_set<std::string>{k1PSIDTSCookieName, k3PSIDTSCookieName},
      /*is_off_the_record_profile_=*/false,
      bound_session_credentials::RotationDebugInfo());
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  SimulateChallengeRequired(CreateChallengeHeaderValue(kChallenge));
  EXPECT_EQ(future.Get(), Result::kSignChallengeFailed);
  VerifyMetricsRecorded(Result::kSignChallengeFailed,
                        /*expect_assertion_was_generated_count=*/1);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       GetResultFromNetErrorAndHttpStatusCode) {
  // Connection error.
  EXPECT_EQ(fetcher_->GetResultFromNetErrorAndHttpStatusCode(
                net::ERR_CONNECTION_TIMED_OUT, std::nullopt),
            Result::kConnectionError);
  // net::OK.
  EXPECT_EQ(
      fetcher_->GetResultFromNetErrorAndHttpStatusCode(net::OK, net::HTTP_OK),
      Result::kSuccess);
  // net::ERR_HTTP_RESPONSE_CODE_FAILURE
  EXPECT_EQ(fetcher_->GetResultFromNetErrorAndHttpStatusCode(
                net::ERR_HTTP_RESPONSE_CODE_FAILURE, net::HTTP_BAD_REQUEST),
            Result::kServerPersistentError);
  // Persistent error.
  EXPECT_EQ(fetcher_->GetResultFromNetErrorAndHttpStatusCode(
                net::OK, net::HTTP_BAD_REQUEST),
            Result::kServerPersistentError);
  EXPECT_EQ(fetcher_->GetResultFromNetErrorAndHttpStatusCode(
                net::OK, net::HTTP_NOT_FOUND),
            Result::kServerPersistentError);
  // Transient error.
  EXPECT_EQ(fetcher_->GetResultFromNetErrorAndHttpStatusCode(
                net::OK, net::HTTP_INTERNAL_SERVER_ERROR),
            Result::kServerTransientError);
  EXPECT_EQ(fetcher_->GetResultFromNetErrorAndHttpStatusCode(
                net::OK, net::HTTP_GATEWAY_TIMEOUT),
            Result::kServerTransientError);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, OnCookiesAccessedRead) {
  EXPECT_FALSE(reported_cookies_notified());
  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kRead);
  EXPECT_FALSE(reported_cookies_notified());
  EXPECT_FALSE(expected_cookies_set());
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, OnCookiesAccessedChange) {
  EXPECT_FALSE(reported_cookies_notified());
  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  EXPECT_TRUE(reported_cookies_notified());
  EXPECT_TRUE(expected_cookies_set());
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, DebugHeaderSent) {
  RotationDebugInfo info;
  RotationDebugInfo::FailureCounter* counter =
      info.add_errors_since_last_rotation();
  counter->set_type(RotationDebugInfo::CONNECTION_ERROR);
  counter->set_count(2);

  fetcher_ = std::make_unique<BoundSessionRefreshCookieFetcherImpl>(
      test_url_loader_factory_.GetSafeWeakWrapper(), *session_binding_helper_,
      kSessionId, /*refresh_url=*/GURL(), kGaiaUrl,
      base::flat_set<std::string>{k1PSIDTSCookieName, k3PSIDTSCookieName},
      /*is_off_the_record_profile_=*/false, info);
  RefreshTestFuture future;
  // Skip some time to create a difference between the the fetcher creation time
  // and the request start time.
  task_environment_.FastForwardBy(base::Seconds(5));
  base::Time time_at_start = base::Time::Now();
  fetcher_->Start(future.GetCallback());
  task_environment_.FastForwardBy(base::Seconds(10));

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);
  auto headers = pending_request->request.headers;
  std::string sent_info_base64;
  ASSERT_TRUE(headers.GetHeader("Sec-Session-Google-Rotation-Debug-Info",
                                &sent_info_base64));

  std::string sent_info_serialized;
  ASSERT_TRUE(base::Base64Decode(sent_info_base64, &sent_info_serialized));
  RotationDebugInfo sent_info;
  ASSERT_TRUE(sent_info.ParseFromString(sent_info_serialized));

  RotationDebugInfo expected_info = info;
  // Fetcher should set the request time when starting the request.
  *expected_info.mutable_request_time() =
      bound_session_credentials::TimeToTimestamp(time_at_start);
  EXPECT_THAT(sent_info, base::test::EqualsProto(expected_info));
}

TEST(BoundSessionRefreshCookieFetcherImplParseChallengeHeaderTest,
     ParseChallengeHeader) {
  auto parse = &BoundSessionRefreshCookieFetcherImpl::ParseChallengeHeader;
  // Empty header.
  EXPECT_THAT(parse(""), FieldsAre("", ""));
  EXPECT_THAT(parse("xyz"), FieldsAre("", ""));
  // Empty challenge field.
  EXPECT_THAT(parse(CreateChallengeHeaderValue("")), FieldsAre("", kSessionId));
  // Empty session ID field.
  EXPECT_THAT(parse(CreateChallengeHeaderValue(kChallenge, "")),
              FieldsAre(kChallenge, ""));
  // Both fields are set.
  EXPECT_THAT(parse(CreateChallengeHeaderValue(kChallenge)),
              FieldsAre(kChallenge, kSessionId));
  EXPECT_THAT(
      parse(CreateChallengeHeaderValue("other_challenge", "other_session_id")),
      FieldsAre("other_challenge", "other_session_id"));
}

class BoundSessionRefreshCookieFetcherImplSignChallengeVerificationTest
    : public BoundSessionRefreshCookieFetcherImplTest {
 public:
  static constexpr std::string_view kGenerateAssertionFirstAttemptHistogram =
      "Signin.BoundSessionCredentials.CookieRotationGenerateAssertionResult."
      "Attempt0";
  static constexpr std::string_view kGenerateAssertionSecondAttemptHistogram =
      "Signin.BoundSessionCredentials.CookieRotationGenerateAssertionResult."
      "Attempt1";

  BoundSessionRefreshCookieFetcherImplSignChallengeVerificationTest() {
    fetcher_.reset();
    // These tests use `MockSessionBindingHelper` to simulate errors more
    // easily.
    auto mock_helper =
        std::make_unique<MockSessionBindingHelper>(unexportable_key_service_);
    mock_session_binding_helper_ = mock_helper.get();
    session_binding_helper_ = std::move(mock_helper);

    fetcher_ = std::make_unique<BoundSessionRefreshCookieFetcherImpl>(
        test_url_loader_factory_.GetSafeWeakWrapper(), *session_binding_helper_,
        kSessionId, /*refresh_url=*/GURL(), kGaiaUrl,
        base::flat_set<std::string>{k1PSIDTSCookieName, k3PSIDTSCookieName},
        /*is_off_the_record_profile_=*/false,
        bound_session_credentials::RotationDebugInfo());
  }

 protected:
  raw_ptr<MockSessionBindingHelper> mock_session_binding_helper_;
};

TEST_F(BoundSessionRefreshCookieFetcherImplSignChallengeVerificationTest,
       FirstAttemptFailedSecondSuccess) {
  const std::string kAssertionToken = "test_token";
  EXPECT_CALL(*mock_session_binding_helper_,
              GenerateBindingKeyAssertion(kChallenge, _, _))
      .WillOnce(RunOnceCallback<2>(base::unexpected(
          SessionBindingHelper::Error::kVerifySignatureFailure)))
      .WillOnce(RunOnceCallback<2>(kAssertionToken));
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());
  SimulateChallengeRequired(CreateChallengeHeaderValue(kChallenge));

  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);
  std::string assertion;
  EXPECT_TRUE(pending_request->request.headers.GetHeader(
      "Sec-Session-Google-Response", &assertion));
  EXPECT_EQ(assertion, kAssertionToken);

  // Set required cookies and complete the request.
  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");

  EXPECT_EQ(future.Get(), Result::kSuccess);
  VerifyMetricsRecorded(BoundSessionRefreshCookieFetcher::Result::kSuccess,
                        /*expect_assertion_was_generated_count=*/2);
  histogram_tester_.ExpectUniqueSample(
      kGenerateAssertionFirstAttemptHistogram,
      SessionBindingHelper::Error::kVerifySignatureFailure, 1);
  histogram_tester_.ExpectUniqueSample(kGenerateAssertionSecondAttemptHistogram,
                                       SessionBindingHelper::kNoErrorForMetrics,
                                       1);
}

TEST_F(BoundSessionRefreshCookieFetcherImplSignChallengeVerificationTest,
       BothAttemptsFailed) {
  EXPECT_CALL(*mock_session_binding_helper_,
              GenerateBindingKeyAssertion(kChallenge, _, _))
      .WillOnce(RunOnceCallback<2>(base::unexpected(
          SessionBindingHelper::Error::kVerifySignatureFailure)))
      .WillOnce(RunOnceCallback<2>(base::unexpected(
          SessionBindingHelper::Error::kVerifySignatureFailure)));
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());
  SimulateChallengeRequired(CreateChallengeHeaderValue(kChallenge));

  EXPECT_EQ(future.Get(), Result::kSignChallengeFailed);
  VerifyMetricsRecorded(
      BoundSessionRefreshCookieFetcher::Result::kSignChallengeFailed,
      /*expect_assertion_was_generated_count=*/2);
  histogram_tester_.ExpectUniqueSample(
      kGenerateAssertionFirstAttemptHistogram,
      SessionBindingHelper::Error::kVerifySignatureFailure, 1);
  histogram_tester_.ExpectUniqueSample(
      kGenerateAssertionSecondAttemptHistogram,
      SessionBindingHelper::Error::kVerifySignatureFailure, 1);
}

TEST_F(BoundSessionRefreshCookieFetcherImplSignChallengeVerificationTest,
       NoRetryForOtherErrors) {
  EXPECT_CALL(*mock_session_binding_helper_,
              GenerateBindingKeyAssertion(kChallenge, _, _))
      .WillOnce(RunOnceCallback<2>(base::unexpected(
          SessionBindingHelper::Error::kAppendSignatureFailure)));
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());
  SimulateChallengeRequired(CreateChallengeHeaderValue(kChallenge));

  EXPECT_EQ(future.Get(), Result::kSignChallengeFailed);
  VerifyMetricsRecorded(
      BoundSessionRefreshCookieFetcher::Result::kSignChallengeFailed,
      /*expect_assertion_was_generated_count=*/1);
  histogram_tester_.ExpectUniqueSample(
      kGenerateAssertionFirstAttemptHistogram,
      SessionBindingHelper::Error::kAppendSignatureFailure, 1);
}

TEST_F(BoundSessionRefreshCookieFetcherImplSignChallengeVerificationTest,
       MixFailedVerificationAndRejectedChallenge) {
  const std::string kAssertionToken = "test_token";
  EXPECT_CALL(*mock_session_binding_helper_,
              GenerateBindingKeyAssertion(kChallenge, _, _))
      .WillOnce(RunOnceCallback<2>(base::unexpected(
          SessionBindingHelper::Error::kVerifySignatureFailure)))
      .WillOnce(RunOnceCallback<2>(kAssertionToken));
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());
  SimulateChallengeRequired(CreateChallengeHeaderValue(kChallenge));

  testing::Mock::VerifyAndClearExpectations(mock_session_binding_helper_);

  // Simulate the server responding with a new challenge.
  const std::string kSecondChallenge = "abcdef";
  EXPECT_CALL(*mock_session_binding_helper_,
              GenerateBindingKeyAssertion(kSecondChallenge, _, _))
      .WillOnce(RunOnceCallback<2>(kAssertionToken));
  SimulateChallengeRequired(CreateChallengeHeaderValue(kSecondChallenge));

  // This time, the rotation request succeeds.
  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");

  EXPECT_EQ(future.Get(), Result::kSuccess);
  VerifyMetricsRecorded(BoundSessionRefreshCookieFetcher::Result::kSuccess,
                        /*expect_assertion_was_generated_count=*/3);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kGenerateAssertionFirstAttemptHistogram),
      ElementsAre(
          base::Bucket(SessionBindingHelper::kNoErrorForMetrics, /*count=*/1),
          base::Bucket(SessionBindingHelper::Error::kVerifySignatureFailure,
                       /*count=*/1)));
  histogram_tester_.ExpectUniqueSample(kGenerateAssertionSecondAttemptHistogram,
                                       SessionBindingHelper::kNoErrorForMetrics,
                                       1);
}
