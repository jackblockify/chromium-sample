// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/data_sharing/data_sharing_ui.h"

#include "chrome/browser/ui/webui/data_sharing/data_sharing_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/data_sharing_resources.h"
#include "chrome/grit/data_sharing_resources_map.h"
#include "components/data_sharing/public/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

DataSharingUIConfig::DataSharingUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  chrome::kChromeUIUntrustedDataSharingHost) {}

DataSharingUIConfig::~DataSharingUIConfig() = default;

std::unique_ptr<content::WebUIController>
DataSharingUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                           const GURL& url) {
  return std::make_unique<DataSharingUI>(web_ui);
}

bool DataSharingUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(
      data_sharing::features::kDataSharingFeature);
}

DataSharingUI::DataSharingUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIUntrustedDataSharingURL);

  webui::SetupWebUIDataSource(
      source, base::make_span(kDataSharingResources, kDataSharingResourcesSize),
      IDR_DATA_SHARING_DATA_SHARING_HTML);
}

DataSharingUI::~DataSharingUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(DataSharingUI)

void DataSharingUI::BindInterface(
    mojo::PendingReceiver<data_sharing::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void DataSharingUI::CreatePageHandler(
    mojo::PendingReceiver<data_sharing::mojom::PageHandler> receiver) {
  page_handler_ =
      std::make_unique<DataSharingPageHandler>(this, std::move(receiver));
}
