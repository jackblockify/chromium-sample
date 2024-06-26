// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_HEURISTICS_OPENER_HEURISTIC_SERVICE_FACTORY_H_
#define CHROME_BROWSER_TPCD_HEURISTICS_OPENER_HEURISTIC_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class OpenerHeuristicService;

class OpenerHeuristicServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static OpenerHeuristicServiceFactory* GetInstance();
  static OpenerHeuristicService* GetForBrowserContext(
      content::BrowserContext* context);

  static ProfileSelections CreateProfileSelections();

 private:
  friend base::NoDestructor<OpenerHeuristicServiceFactory>;

  OpenerHeuristicServiceFactory();
  ~OpenerHeuristicServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_TPCD_HEURISTICS_OPENER_HEURISTIC_SERVICE_FACTORY_H_
