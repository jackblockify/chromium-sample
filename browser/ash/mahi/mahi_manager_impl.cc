// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/mahi_manager_impl.h"

#include <stdint.h>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/system/mahi/mahi_ui_controller.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "ash/webui/settings/public/constants/setting.mojom.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/mahi/mahi_browser_delegate_ash.h"
#include "chrome/browser/ash/mahi/mahi_cache_manager.h"
#include "chrome/browser/manta/manta_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/components/mahi/public/cpp/mahi_media_app_content_manager.h"
#include "chromeos/crosapi/mojom/mahi.mojom-forward.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/feedback/feedback_constants.h"
#include "components/manta/features.h"
#include "components/manta/manta_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace {

using chromeos::MahiResponseStatus;
using crosapi::mojom::MahiContextMenuActionType;

const char kMahiCacheHit[] = "ChromeOS.Mahi.CacheStateOnAccess";
const char kMahiResponseStatus[] = "ChromeOS.Mahi.ResponseStatusOnRequest";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CacheHit {
  kNoHit = 0,
  kSummary = 1,
  kContent = 2,
  kMaxValue = kContent,
};

MahiResponseStatus GetMahiResponseStatusFromMantaStatus(
    manta::MantaStatusCode code) {
  switch (code) {
    case manta::MantaStatusCode::kOk:
      return MahiResponseStatus::kSuccess;
    case manta::MantaStatusCode::kGenericError:
    case manta::MantaStatusCode::kBackendFailure:
    case manta::MantaStatusCode::kNoInternetConnection:
    case manta::MantaStatusCode::kNoIdentityManager:
      return MahiResponseStatus::kUnknownError;
    case manta::MantaStatusCode::kRestrictedCountry:
      return MahiResponseStatus::kRestrictedCountry;
    case manta::MantaStatusCode::kUnsupportedLanguage:
      return MahiResponseStatus::kUnsupportedLanguage;
    case manta::MantaStatusCode::kBlockedOutputs:
      return MahiResponseStatus::kInappropriate;
    case manta::MantaStatusCode::kResourceExhausted:
      return MahiResponseStatus::kResourceExhausted;
    case manta::MantaStatusCode::kPerUserQuotaExceeded:
      return MahiResponseStatus::kQuotaLimitHit;
    default:
      return MahiResponseStatus::kUnknownError;
  }
}

std::unique_ptr<manta::MahiProvider> CreateProvider() {
  if (!manta::features::IsMantaServiceEnabled()) {
    return nullptr;
  }

  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile) {
    return nullptr;
  }

  if (manta::MantaService* service =
          manta::MantaServiceFactory::GetForProfile(profile)) {
    return service->CreateMahiProvider();
  }

  return nullptr;
}

}  // namespace

namespace ash {

MahiManagerImpl::MahiManagerImpl()
    : cache_manager_(std::make_unique<MahiCacheManager>()) {
  session_observation_.Observe(Shell::Get()->session_controller());
  PrefService* last_active_user_pref_service =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (last_active_user_pref_service) {
    OnActiveUserPrefServiceChanged(last_active_user_pref_service);
  }
}

MahiManagerImpl::~MahiManagerImpl() {
  mahi_provider_.reset();
  mahi_browser_delegate_ash_ = nullptr;
}

std::u16string MahiManagerImpl::GetContentTitle() {
  return current_page_info_->title;
}

gfx::ImageSkia MahiManagerImpl::GetContentIcon() {
  return current_page_info_->favicon_image;
}

GURL MahiManagerImpl::GetContentUrl() {
  return current_page_info_->url;
}

void MahiManagerImpl::GetSummary(MahiSummaryCallback callback) {
  if (!MaybeInitializeAndDiscardPendingRequests()) {
    latest_response_status_ = MahiResponseStatus::kUnknownError;
    std::move(callback).Run(u"", latest_response_status_);
    LOG(ERROR) << "Initialized unsuccessfully.";
    return;
  }

  current_panel_info_ = current_page_info_->Clone();

  // Uses the summary if it is already in the cache.
  const auto cached_summary =
      cache_manager_->GetSummaryForUrl(current_panel_info_->url.spec());
  const auto cached_content =
      cache_manager_->GetPageContentForUrl(current_panel_info_->url.spec());
  if (cached_summary.has_value()) {
    current_panel_content_ = crosapi::mojom::MahiPageContent::New(
        /*client_id=*/base::UnguessableToken(),
        /*page_id=*/base::UnguessableToken(), cached_content);
    current_panel_qa_.clear();

    // TODO(b:338140794): consider loading the QA cache here as well.

    latest_summary_ = cached_summary.value();
    latest_response_status_ = MahiResponseStatus::kSuccess;
    std::move(callback).Run(cached_summary.value(),
                            MahiResponseStatus::kSuccess);

    base::UmaHistogramEnumeration(kMahiCacheHit, CacheHit::kSummary);
    return;
  }

  // Uses page content if it is already in the cache.
  if (!cached_content.empty()) {
    OnGetPageContentForSummary(
        current_panel_info_->Clone(), std::move(callback),
        crosapi::mojom::MahiPageContent::New(
            /*client_id=*/base::UnguessableToken(),
            /*page_id=*/base::UnguessableToken(), cached_content));

    base::UmaHistogramEnumeration(kMahiCacheHit, CacheHit::kContent);
    return;
  }

  base::UmaHistogramEnumeration(kMahiCacheHit, CacheHit::kNoHit);
  auto get_content_done_callback =
      base::BindOnce(&MahiManagerImpl::OnGetPageContentForSummary,
                     weak_ptr_factory_for_requests_.GetWeakPtr(),
                     current_panel_info_->Clone(), std::move(callback));

  if (media_app_pdf_focused_) {
    chromeos::MahiMediaAppContentManager::Get()->GetContent(
        media_app_client_id_, std::move(get_content_done_callback));
  } else {
    mahi_browser_delegate_ash_->GetContentFromClient(
        current_panel_info_->client_id, current_panel_info_->page_id,
        std::move(get_content_done_callback));
  }
}

void MahiManagerImpl::GetOutlines(MahiOutlinesCallback callback) {
  std::vector<chromeos::MahiOutline> outlines;
  for (int i = 0; i < 5; i++) {
    outlines.emplace_back(
        chromeos::MahiOutline(i, u"Outline " + base::NumberToString16(i)));
  }
  std::move(callback).Run(outlines, MahiResponseStatus::kSuccess);
}

void MahiManagerImpl::GoToOutlineContent(int outline_id) {}

void MahiManagerImpl::AnswerQuestion(const std::u16string& question,
                                     bool current_panel_content,
                                     MahiAnswerQuestionCallback callback) {
  if (!MaybeInitializeAndDiscardPendingRequests()) {
    latest_response_status_ = MahiResponseStatus::kUnknownError;
    std::move(callback).Run(u"", latest_response_status_);
    LOG(ERROR) << "Initialized unsuccessfully.";
    return;
  }

  if (current_panel_content) {
    mahi_provider_->QuestionAndAnswer(
        base::UTF16ToUTF8(current_panel_content_->page_content),
        current_panel_qa_, base::UTF16ToUTF8(question),
        base::BindOnce(&MahiManagerImpl::OnMahiProviderQAResponse,
                       weak_ptr_factory_for_requests_.GetWeakPtr(),
                       current_panel_info_->Clone(), question,
                       std::move(callback)));
    return;
  }

  current_panel_info_ = current_page_info_->Clone();

  // Uses page content if it is already in the cache.
  const auto cached_content =
      cache_manager_->GetPageContentForUrl(current_panel_info_->url.spec());
  if (!cached_content.empty()) {
    OnGetPageContentForQA(
        current_panel_info_->Clone(), question, std::move(callback),
        crosapi::mojom::MahiPageContent::New(
            /*client_id=*/base::UnguessableToken(),
            /*page_id=*/base::UnguessableToken(), cached_content));

    base::UmaHistogramEnumeration(kMahiCacheHit, CacheHit::kContent);
    return;
  }

  base::UmaHistogramEnumeration(kMahiCacheHit, CacheHit::kNoHit);
  auto get_content_done_callback = base::BindOnce(
      &MahiManagerImpl::OnGetPageContentForQA,
      weak_ptr_factory_for_requests_.GetWeakPtr(), current_panel_info_->Clone(),
      question, std::move(callback));
  if (media_app_pdf_focused_) {
    chromeos::MahiMediaAppContentManager::Get()->GetContent(
        media_app_client_id_, std::move(get_content_done_callback));
  } else {
    mahi_browser_delegate_ash_->GetContentFromClient(
        current_panel_info_->client_id, current_panel_info_->page_id,
        std::move(get_content_done_callback));
  }
}

void MahiManagerImpl::GetSuggestedQuestion(
    MahiGetSuggestedQuestionCallback callback) {
  std::move(callback).Run(u"test suggested question",
                          MahiResponseStatus::kSuccess);
}

void MahiManagerImpl::SetCurrentFocusedPageInfo(
    crosapi::mojom::MahiPageInfoPtr info) {
  media_app_pdf_focused_ = false;
  // TODO(b/318565610): consider adding default icon when there is no icon
  // available.
  current_page_info_ = std::move(info);

  const bool availability =
      current_page_info_->IsDistillable.value_or(false) &&
      !current_panel_info_->url.EqualsIgnoringRef(current_page_info_->url);
  NotifyRefreshAvailability(/*available=*/availability);
}

void MahiManagerImpl::OnContextMenuClicked(
    crosapi::mojom::MahiContextMenuRequestPtr context_menu_request) {
  switch (context_menu_request->action_type) {
    case MahiContextMenuActionType::kSummary:
    case MahiContextMenuActionType::kOutline:
      // TODO(b/318565610): Update the behaviour of kOutline.
      ui_controller_.OpenMahiPanel(
          context_menu_request->display_id,
          context_menu_request->mahi_menu_bounds.has_value()
              ? context_menu_request->mahi_menu_bounds.value()
              : gfx::Rect());
      return;
    case MahiContextMenuActionType::kQA:
      ui_controller_.OpenMahiPanel(
          context_menu_request->display_id,
          context_menu_request->mahi_menu_bounds.has_value()
              ? context_menu_request->mahi_menu_bounds.value()
              : gfx::Rect());

      // Ask question.
      // TODO(b/331837721): `MahiManagerImpl` should own an instance of
      // `MahiUiController` and use it to answer question here. This
      // functionality shouldn't need to be routed through the widget. We also
      // need to add unit test logic for this after the refactor.
      if (!context_menu_request->question) {
        return;
      }

      if (!ui_controller_.IsMahiPanelOpen()) {
        return;
      }

      // Because we call `MahiUiController::SendQuestion` right after
      // opening the panel here, `SendQuestion` will cancel the call to get
      // summary due to `MahiUiController::InvalidatePendingRequests()`. Thus,
      // we need to update the summary after answering the question to make sure
      // that user gets summary when navigating back to the summary UI
      // (b/345621992).
      ui_controller_.SetUpdateSummaryAfterAnswerQuestion();

      // When the user sends a question from the context menu, we treat it as
      // the start of a new journey, so we set `current_panel_content` false.
      ui_controller_.SendQuestion(context_menu_request->question.value(),
                                  /*current_panel_content=*/false,
                                  MahiUiController::QuestionSource::kMenuView);

      return;
    case MahiContextMenuActionType::kSettings:
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
          ProfileManager::GetActiveUserProfile(),
          chromeos::settings::mojom::kSystemPreferencesSectionPath,
          chromeos::settings::mojom::Setting::kMahiOnOff);
      return;
    case MahiContextMenuActionType::kNone:
      return;
  }
}

void MahiManagerImpl::OpenFeedbackDialog() {
  std::string description_template = base::StringPrintf(
      "#Mahi user feedback:\n\n-----------\nlatest status code: %d\nlatest "
      "summary: %s",
      static_cast<int>(latest_response_status_),
      base::UTF16ToUTF8(latest_summary_).c_str());

  if (!current_panel_qa_.empty()) {
    base::StringAppendF(&description_template, "\nQA history:");
    for (const auto& [question, answer] : current_panel_qa_) {
      base::StringAppendF(&description_template, "\nQ:%s\nA:%s\n",
                          question.c_str(), answer.c_str());
    }
  }

  base::Value::Dict ai_metadata;
  ai_metadata.Set(feedback::kMahiMetadataKey, "true");

  chrome::ShowFeedbackPage(
      /*browser=*/chrome::FindBrowserWithProfile(
          ProfileManager::GetActiveUserProfile()),
      /*source=*/feedback::kFeedbackSourceAI, description_template,
      /*description_placeholder_text=*/
      base::UTF16ToUTF8(
          l10n_util::GetStringUTF16(IDS_MAHI_FEEDBACK_PLACEHOLDER)),
      /*category_tag=*/"mahi",
      /*extra_diagnostics=*/std::string(),
      /*autofill_metadata=*/base::Value::Dict(), std::move(ai_metadata));
}

bool MahiManagerImpl::IsEnabled() {
  return IsSupportedWithCorrectFeatureKey() &&
         Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
             ash::prefs::kMahiEnabled);
}

void MahiManagerImpl::SetMediaAppPDFFocused() {
  chromeos::MahiMediaAppContentManager* media_app_content_manager =
      chromeos::MahiMediaAppContentManager::Get();
  CHECK(media_app_content_manager);

  bool old_media_app_pdf_focused = media_app_pdf_focused_;
  base::UnguessableToken old_media_app_client_id = media_app_client_id_;
  const std::u16string old_title = current_page_info_->title;

  media_app_client_id_ = media_app_content_manager->active_client_id();
  media_app_pdf_focused_ = true;
  std::optional<std::string> file_name =
      media_app_content_manager->GetFileName(media_app_client_id_);
  CHECK(file_name.has_value());

  // Fits the media app page info into a MahiPageInfoPtr.
  // Particularly, makes up a GURL with the file name.
  // TODO(b:338140794): Two file with the same name can hit the same cache. Need
  // to find a way to fix this.
  current_page_info_ = crosapi::mojom::MahiPageInfo::New(
      media_app_client_id_,
      /*page_id=*/media_app_client_id_,
      GURL{base::StrCat({"file:///media-app/", file_name.value()})},
      /*title=*/base::UTF8ToUTF16(file_name.value()), gfx::ImageSkia(),
      /*distillable=*/true);

  // To avoid refresh banner flicker. This could happen when a new PDF file is
  // opened from file picker dialog in media app.
  if (old_media_app_pdf_focused &&
      old_media_app_client_id == media_app_client_id_ &&
      current_page_info_->title == old_title) {
    return;
  }

  const bool availability =
      !current_panel_info_->url.EqualsIgnoringRef(current_page_info_->url);
  NotifyRefreshAvailability(/*available=*/availability);
}

void MahiManagerImpl::MediaAppPDFClosed(
    const base::UnguessableToken media_app_client_id) {
  if (media_app_pdf_focused_ && media_app_client_id_ == media_app_client_id &&
      current_page_info_->client_id == media_app_client_id) {
    // In this case if there's a refresh banner, it must be targeted to
    // the destroying media app PDF. Hides it by a false notification.
    NotifyRefreshAvailability(/*available=*/false);
    current_page_info_ = crosapi::mojom::MahiPageInfo::New();
  }

  media_app_pdf_focused_ = false;
  media_app_client_id_ = base::UnguessableToken::Null();
}

std::optional<base::UnguessableToken> MahiManagerImpl::GetMediaAppPDFClientId()
    const {
  if (media_app_pdf_focused_) {
    return media_app_client_id_;
  }
  return std::nullopt;
}

void MahiManagerImpl::NotifyRefreshAvailability(bool available) {
  if (ui_controller_.IsMahiPanelOpen()) {
    ui_controller_.NotifyRefreshAvailabilityChanged(available);
  }
}

void MahiManagerImpl::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  CHECK(pref_service);
  // Subscribes again to pref changes.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      ash::prefs::kMahiEnabled,
      base::BindRepeating(&MahiManagerImpl::OnMahiPrefChanged,
                          weak_ptr_factory_for_pref_.GetWeakPtr()));

  OnMahiPrefChanged();
}

void MahiManagerImpl::OnMahiPrefChanged() {
  CHECK(pref_change_registrar_);
  CHECK(pref_change_registrar_->prefs());
  if (!pref_change_registrar_->prefs()->GetBoolean(ash::prefs::kMahiEnabled)) {
    ui_controller_.CloseMahiPanel();
    cache_manager_->ClearCache();
  }
}

bool MahiManagerImpl::MaybeInitializeAndDiscardPendingRequests() {
  if (!mahi_provider_) {
    mahi_provider_ = CreateProvider();
  }

  if (!mahi_browser_delegate_ash_) {
    mahi_browser_delegate_ash_ = crosapi::CrosapiManager::Get()
                                     ->crosapi_ash()
                                     ->mahi_browser_delegate_ash();
  }

  if (weak_ptr_factory_for_requests_.HasWeakPtrs()) {
    weak_ptr_factory_for_requests_.InvalidateWeakPtrs();
  }

  return mahi_provider_ && mahi_browser_delegate_ash_;
}

void MahiManagerImpl::OnGetPageContentForSummary(
    crosapi::mojom::MahiPageInfoPtr request_page_info,
    MahiSummaryCallback callback,
    crosapi::mojom::MahiPageContentPtr mahi_content_ptr) {
  if (!mahi_content_ptr || mahi_content_ptr->page_content.empty()) {
    latest_response_status_ = MahiResponseStatus::kContentExtractionError;
    std::move(callback).Run(u"", latest_response_status_);
    base::UmaHistogramEnumeration(kMahiResponseStatus, latest_response_status_);
    return;
  }

  // Assign current panel content and clear the current panel QA
  current_panel_content_ = std::move(mahi_content_ptr);
  current_panel_qa_.clear();

  // Add page content to the cache.
  // TODO(b:338140794): consider adding the QA to the cache.
  cache_manager_->AddCacheForUrl(
      request_page_info->url.spec(),
      MahiCacheManager::MahiData(
          request_page_info->url.spec(), request_page_info->title,
          current_panel_content_->page_content,
          request_page_info->favicon_image, /*summary=*/std::nullopt,
          /*previous_qa=*/{}));

  CHECK(mahi_provider_);
  mahi_provider_->Summarize(
      base::UTF16ToUTF8(current_panel_content_->page_content),
      base::BindOnce(&MahiManagerImpl::OnMahiProviderSummaryResponse,
                     weak_ptr_factory_for_requests_.GetWeakPtr(),
                     std::move(request_page_info), std::move(callback)));
}

void MahiManagerImpl::OnGetPageContentForQA(
    crosapi::mojom::MahiPageInfoPtr request_page_info,
    const std::u16string& question,
    MahiAnswerQuestionCallback callback,
    crosapi::mojom::MahiPageContentPtr mahi_content_ptr) {
  if (!mahi_content_ptr || mahi_content_ptr->page_content.empty()) {
    latest_response_status_ = MahiResponseStatus::kContentExtractionError;
    std::move(callback).Run(std::nullopt, latest_response_status_);
    base::UmaHistogramEnumeration(kMahiResponseStatus, latest_response_status_);
    return;
  }

  // Assign current panel content and clear the current panel QA
  current_panel_content_ = std::move(mahi_content_ptr);
  current_panel_qa_.clear();

  // Add page content to the cache. The summary would be the summary that
  // is already in the cache (if any).
  // TODO(b:338140794): consider adding the QA to the cache.
  cache_manager_->AddCacheForUrl(
      request_page_info->url.spec(),
      MahiCacheManager::MahiData(
          request_page_info->url.spec(), request_page_info->title,
          current_panel_content_->page_content,
          request_page_info->favicon_image,
          cache_manager_->GetSummaryForUrl(request_page_info->url.spec()), {}));

  mahi_provider_->QuestionAndAnswer(
      base::UTF16ToUTF8(current_panel_content_->page_content),
      current_panel_qa_, base::UTF16ToUTF8(question),
      base::BindOnce(&MahiManagerImpl::OnMahiProviderQAResponse,
                     weak_ptr_factory_for_requests_.GetWeakPtr(),
                     std::move(request_page_info), question,
                     std::move(callback)));
}

void MahiManagerImpl::OnMahiProviderSummaryResponse(
    crosapi::mojom::MahiPageInfoPtr request_page_info,
    MahiSummaryCallback summary_callback,
    base::Value::Dict dict,
    manta::MantaStatus status) {
  latest_summary_ = u"...";
  if (status.status_code != manta::MantaStatusCode::kOk) {
    latest_response_status_ =
        GetMahiResponseStatusFromMantaStatus(status.status_code);
    std::move(summary_callback)
        .Run(u"Couldn't get summary", latest_response_status_);
    base::UmaHistogramEnumeration(kMahiResponseStatus, latest_response_status_);
    return;
  }

  if (auto* text = dict.FindString("outputData")) {
    latest_response_status_ = MahiResponseStatus::kSuccess;
    latest_summary_ = base::UTF8ToUTF16(*text);

    cache_manager_->TryToUpdateSummaryForUrl(request_page_info->url.spec(),
                                             latest_summary_);

    std::move(summary_callback).Run(latest_summary_, latest_response_status_);
  } else {
    latest_response_status_ = MahiResponseStatus::kCantFindOutputData;
    std::move(summary_callback)
        .Run(u"Cannot find output data", latest_response_status_);
  }
  base::UmaHistogramEnumeration(kMahiResponseStatus, latest_response_status_);
}

void MahiManagerImpl::OnMahiProviderQAResponse(
    crosapi::mojom::MahiPageInfoPtr request_page_info,
    const std::u16string& question,
    MahiAnswerQuestionCallback callback,
    base::Value::Dict dict,
    manta::MantaStatus status) {
  if (status.status_code != manta::MantaStatusCode::kOk) {
    latest_response_status_ =
        GetMahiResponseStatusFromMantaStatus(status.status_code);
    current_panel_qa_.emplace_back(base::UTF16ToUTF8(question), "");
    std::move(callback).Run(std::nullopt, latest_response_status_);
    base::UmaHistogramEnumeration(kMahiResponseStatus, latest_response_status_);
    return;
  }

  if (auto* text = dict.FindString("outputData")) {
    latest_response_status_ = MahiResponseStatus::kSuccess;
    current_panel_qa_.emplace_back(base::UTF16ToUTF8(question), *text);
    std::move(callback).Run(base::UTF8ToUTF16(*text), latest_response_status_);
  } else {
    latest_response_status_ = MahiResponseStatus::kCantFindOutputData;
    std::move(callback).Run(std::nullopt, latest_response_status_);
  }
  base::UmaHistogramEnumeration(kMahiResponseStatus, latest_response_status_);
}

}  // namespace ash
