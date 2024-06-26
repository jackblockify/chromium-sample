// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPANION_VISUAL_QUERY_VISUAL_QUERY_SUGGESTIONS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_COMPANION_VISUAL_QUERY_VISUAL_QUERY_SUGGESTIONS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/companion/visual_query/visual_query_suggestions_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

class KeyedService;
class Profile;

namespace companion::visual_query {
class VisualQuerySuggestionsService;

// Singleton that owns VisualQuerySuggestionsService objects, one for each
// active Profile.
class VisualQuerySuggestionsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Creates the service if it does not already exist for the profile.
  static VisualQuerySuggestionsService* GetForProfile(Profile* profile);

  // Get the singleton instance
  static VisualQuerySuggestionsServiceFactory* GetInstance();

  VisualQuerySuggestionsServiceFactory(
      const VisualQuerySuggestionsServiceFactory&) = delete;

  VisualQuerySuggestionsServiceFactory& operator=(
      const VisualQuerySuggestionsServiceFactory&) = delete;

 private:
  friend base::NoDestructor<VisualQuerySuggestionsServiceFactory>;

  VisualQuerySuggestionsServiceFactory();

  ~VisualQuerySuggestionsServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace companion::visual_query

#endif  // CHROME_BROWSER_COMPANION_VISUAL_QUERY_VISUAL_QUERY_SUGGESTIONS_SERVICE_FACTORY_H_
