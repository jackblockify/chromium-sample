// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/service_worker_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_transport_simple_test_server.h"
#include "extensions/browser/api/web_request/extension_web_request_event_router.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace controlled_frame {

namespace {

constexpr char kWebRequestOnBeforeRequestEventName[] =
    "webViewInternal.onBeforeRequest";
constexpr char kWebRequestOnAuthRequiredEventName[] =
    "webViewInternal.onAuthRequired";
constexpr char kEvalSuccessStr[] = "SUCCESS";

const extensions::MenuItem::Id CreateMenuItemId(
    const extensions::MenuItem::ExtensionKey& extension_key,
    const std::string& string_uid) {
  extensions::MenuItem::Id id;
  id.extension_key = extension_key;
  id.string_uid = string_uid;
  return id;
}

const content::EvalJsResult CreateContextMenuItem(
    content::RenderFrameHost* app_frame,
    const std::string& id,
    const std::string& title) {
  return content::EvalJs(app_frame, content::JsReplace(R"(
      new Promise((resolve, reject) => {
        const frame = document.getElementsByTagName('controlledframe')[0];
        if (!frame || !frame.contextMenus || !frame.contextMenus.create) {
          reject('FAIL: frame, frame.contextMenus, or ' +
              'frame.contextMenus.create is undefined');
          return;
        }
        frame.contextMenus.create(
            { title: $2, id: $1 },
            () => { resolve('SUCCESS'); });
      });
    )",
                                                       id, title));
}

const content::EvalJsResult UpdateContextMenuItemTitle(
    content::RenderFrameHost* app_frame,
    const std::string& id,
    const std::string& new_title) {
  return content::EvalJs(app_frame, content::JsReplace(R"(
      new Promise((resolve, reject) => {
        const frame = document.getElementsByTagName('controlledframe')[0];
        if (!frame || !frame.contextMenus || !frame.contextMenus.update) {
          reject('FAIL: frame, frame.contextMenus, or ' +
              'frame.contextMenus.update is undefined');
          return;
        }

        frame.contextMenus.update(
            /*id=*/$1,
            { title: $2 },
            () => { resolve('SUCCESS'); });
      });
  )",
                                                       id, new_title));
}

const content::EvalJsResult RemoveContextMenuItem(
    content::RenderFrameHost* app_frame,
    const std::string& id) {
  return content::EvalJs(app_frame, content::JsReplace(R"(
      new Promise((resolve, reject) => {
        const frame = document.getElementsByTagName('controlledframe')[0];
        if (!frame || !frame.contextMenus || !frame.contextMenus.remove) {
          reject('FAIL: frame, frame.contextMenus, or ' +
              'frame.contextMenus.remove is undefined');
          return;
        }

        frame.contextMenus.remove(
            /*id=*/$1,
            () => { resolve('SUCCESS'); });
      });
  )",
                                                       id));
}

const content::EvalJsResult RemoveAllContextMenuItems(
    content::RenderFrameHost* app_frame) {
  return content::EvalJs(app_frame, R"(
      new Promise((resolve, reject) => {
        const frame = document.getElementsByTagName('controlledframe')[0];
        if (!frame || !frame.contextMenus || !frame.contextMenus.removeAll) {
          reject('FAIL: frame, frame.contextMenus, or ' +
              'frame.contextMenus.removeAll is undefined');
          return;
        }

        frame.contextMenus.removeAll(() => { resolve('SUCCESS'); });
      });
  )");
}

const content::EvalJsResult SetBackgroundColorToWhite(
    extensions::WebViewGuest* guest) {
  return content::EvalJs(guest->GetGuestMainFrame(), R"(
    (function() {
      document.body.style.backgroundColor = 'white';
      return 'SUCCESS';
    })();
  )");
}

const content::EvalJsResult ExecuteScriptRedBackgroundCode(
    content::RenderFrameHost* app_frame) {
  return content::EvalJs(app_frame, R"(
      new Promise((resolve, reject) => {
        const frame = document.getElementsByTagName('controlledframe')[0];
        if (!frame || !frame.request) {
          reject('FAIL');
          return;
        }
        frame.executeScript(
          {code: "document.body.style.backgroundColor = 'red';"},
          () => { resolve('SUCCESS') });
      });
  )");
}

const content::EvalJsResult ExecuteScriptRedBackgroundFile(
    content::RenderFrameHost* app_frame) {
  return content::EvalJs(app_frame, R"(
      new Promise((resolve, reject) => {
        const frame = document.getElementsByTagName('controlledframe')[0];
        if (!frame || !frame.request) {
          reject('FAIL');
          return;
        }
        frame.executeScript(
          {file: "/execute_script.input.js"},
          () => { resolve('SUCCESS') });
      });
  )");
}

const content::EvalJsResult VerifyBackgroundColorIsRed(
    extensions::WebViewGuest* guest) {
  return content::EvalJs(guest->GetGuestMainFrame(), R"(
    (function() {
      if (document.body.style.backgroundColor === 'red') {
        return 'SUCCESS';
      } else {
        return 'FAIL';
      }
    })();
  )");
}

[[nodiscard]] bool IsControlledFramePresent(
    content::RenderFrameHost* app_frame) {
  return ExecJs(app_frame, R"(
      new Promise((resolve, reject) => {
        const controlledframe = document.createElement('controlledframe');
        if (('src' in controlledframe)) {
          // Tag is defined.
          resolve('SUCCESS');
        } else {
          reject('FAIL');
        }
      });
  )");
}

// TODO(odejesush): Add tests for the rest of the Promise API methods.
const char* kControlledFramePromiseApiMethods[]{"back", "forward", "go"};

}  // namespace

class ControlledFrameApiTest
    : public web_app::IsolatedWebAppBrowserTestHarness {
 public:
  void SetUpOnMainThread() override {
    web_app::IsolatedWebAppBrowserTestHarness::SetUpOnMainThread();
    embedded_https_test_server().ServeFilesFromSourceDirectory(
        GetChromeTestDataDir().AppendASCII("web_apps/simple_isolated_app"));
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  web_app::IsolatedWebAppUrlInfo CreateAndInstallEmptyApp(
      const web_app::ManifestBuilder& manifest_builder) {
    app_ = web_app::IsolatedWebAppBuilder(manifest_builder).BuildBundle();
    app_->TrustSigningKey();
    base::expected<web_app::IsolatedWebAppUrlInfo, std::string> url_info =
        app_->Install(profile());
    CHECK(url_info.has_value()) << url_info.error();
    return url_info.value();
  }

  [[nodiscard]] bool CreateControlledFrame(content::RenderFrameHost* frame,
                                           const GURL& src) {
    static std::string kCreateControlledFrame = R"(
        new Promise((resolve, reject) => {
          const controlledframe = document.createElement('controlledframe');
          if (!('src' in controlledframe)) {
            // Tag is undefined or generates a malformed response.
            reject('FAIL');
            return;
          }
          controlledframe.setAttribute('src', $1);
          controlledframe.addEventListener('loadstop', resolve);
          controlledframe.addEventListener('loadabort', reject);
          document.body.appendChild(controlledframe);
        });
    )";
    return ExecJs(frame, content::JsReplace(kCreateControlledFrame, src));
  }

  extensions::WebViewGuest* GetWebViewGuest(
      content::RenderFrameHost* embedder_frame) {
    extensions::WebViewGuest* web_view_guest = nullptr;
    embedder_frame->ForEachRenderFrameHostWithAction(
        [&web_view_guest](content::RenderFrameHost* rfh) {
          if (auto* web_view =
                  extensions::WebViewGuest::FromRenderFrameHost(rfh)) {
            web_view_guest = web_view;
            return content::RenderFrameHost::FrameIterationAction::kStop;
          }
          return content::RenderFrameHost::FrameIterationAction::kContinue;
        });
    return web_view_guest;
  }

  void ExpectMenuItemWithIdAndTitle(
      const extensions::MenuItem::ExtensionKey& extension_key,
      const std::string& expected_id,
      const std::string& expected_title) {
    auto* menu_manager = extensions::MenuManager::Get(profile());
    extensions::MenuItem* menu_item =
        menu_manager->GetItemById(CreateMenuItemId(extension_key, expected_id));

    ASSERT_TRUE(menu_item);
    EXPECT_EQ(expected_title, menu_item->title());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kIsolateSandboxedIframes};
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app_;
};

IN_PROC_BROWSER_TEST_F(ControlledFrameApiTest, ContextMenusCreate) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL("/index.html")));
  extensions::WebViewGuest* web_view_guest = GetWebViewGuest(app_frame);
  auto* menu_manager = extensions::MenuManager::Get(profile());

  const extensions::MenuItem::ExtensionKey extension_key(
      /*extension_id=*/"", web_view_guest->owner_rfh()->GetProcess()->GetID(),
      web_view_guest->owner_rfh()->GetRoutingID(),
      web_view_guest->view_instance_id());
  EXPECT_EQ(0u, menu_manager->MenuItemsSize(extension_key));

  static constexpr std::string kItem1ID = "1";
  static constexpr std::string kItem1Title = "Test";
  EXPECT_EQ(kEvalSuccessStr,
            CreateContextMenuItem(app_frame, kItem1ID, kItem1Title));
  ASSERT_EQ(1u, menu_manager->MenuItemsSize(extension_key));
  ExpectMenuItemWithIdAndTitle(extension_key, kItem1ID, kItem1Title);

  static constexpr std::string kItem2ID = "2";
  static constexpr std::string kItem2Title = "Test2";
  EXPECT_EQ(kEvalSuccessStr,
            CreateContextMenuItem(app_frame, kItem2ID, kItem2Title));
  ASSERT_EQ(2u, menu_manager->MenuItemsSize(extension_key));
  ExpectMenuItemWithIdAndTitle(extension_key, kItem2ID, kItem2Title);

  static constexpr std::string kItem3ID = "3";
  static constexpr std::string kItem3Title = "Test3";
  EXPECT_EQ(kEvalSuccessStr,
            CreateContextMenuItem(app_frame, kItem3ID, kItem3Title));
  ASSERT_EQ(3u, menu_manager->MenuItemsSize(extension_key));
  ExpectMenuItemWithIdAndTitle(extension_key, kItem3ID, kItem3Title);
}

IN_PROC_BROWSER_TEST_F(ControlledFrameApiTest, ContextMenusUpdate) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL("/index.html")));
  extensions::WebViewGuest* web_view_guest = GetWebViewGuest(app_frame);
  auto* menu_manager = extensions::MenuManager::Get(profile());

  static constexpr std::string kItem1ID = "1";
  static constexpr std::string kItem1Title = "Test";
  EXPECT_EQ(kEvalSuccessStr,
            CreateContextMenuItem(app_frame, kItem1ID, kItem1Title));

  const extensions::MenuItem::ExtensionKey extension_key(
      /*extension_id=*/"", web_view_guest->owner_rfh()->GetProcess()->GetID(),
      web_view_guest->owner_rfh()->GetRoutingID(),
      web_view_guest->view_instance_id());
  ASSERT_EQ(1u, menu_manager->MenuItemsSize(extension_key));
  ExpectMenuItemWithIdAndTitle(extension_key, kItem1ID, kItem1Title);

  static constexpr std::string kItem1NewTitle = "Test1";
  EXPECT_EQ(kEvalSuccessStr,
            UpdateContextMenuItemTitle(app_frame, kItem1ID, kItem1NewTitle));

  ASSERT_EQ(1u, menu_manager->MenuItemsSize(extension_key));
  ExpectMenuItemWithIdAndTitle(extension_key, kItem1ID, kItem1NewTitle);
}

IN_PROC_BROWSER_TEST_F(ControlledFrameApiTest, ContextMenusRemove) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL("/index.html")));
  extensions::WebViewGuest* web_view_guest = GetWebViewGuest(app_frame);
  auto* menu_manager = extensions::MenuManager::Get(profile());

  static constexpr std::string kItem1ID = "1";
  static constexpr std::string kItem1Title = "Test1";
  EXPECT_EQ(kEvalSuccessStr,
            CreateContextMenuItem(app_frame, kItem1ID, kItem1Title));
  EXPECT_EQ(kEvalSuccessStr, CreateContextMenuItem(app_frame, /*id=*/"2",
                                                   /*title=*/"Test2"));

  EXPECT_EQ(kEvalSuccessStr, RemoveContextMenuItem(app_frame, kItem1ID));

  const extensions::MenuItem::ExtensionKey extension_key(
      /*extension_id=*/"", web_view_guest->owner_rfh()->GetProcess()->GetID(),
      web_view_guest->owner_rfh()->GetRoutingID(),
      web_view_guest->view_instance_id());
  ASSERT_EQ(1u, menu_manager->MenuItemsSize(extension_key));

  extensions::MenuItem* deleted_item =
      menu_manager->GetItemById(CreateMenuItemId(extension_key, kItem1ID));
  EXPECT_FALSE(deleted_item);
}

IN_PROC_BROWSER_TEST_F(ControlledFrameApiTest, ContextMenusRemoveAll) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL("/index.html")));
  extensions::WebViewGuest* web_view_guest = GetWebViewGuest(app_frame);
  auto* menu_manager = extensions::MenuManager::Get(profile());

  EXPECT_EQ(kEvalSuccessStr, CreateContextMenuItem(app_frame, /*id=*/"1",
                                                   /*title=*/"Test1"));
  EXPECT_EQ(kEvalSuccessStr, CreateContextMenuItem(app_frame, /*id=*/"2",
                                                   /*title=*/"Test2"));

  EXPECT_EQ(kEvalSuccessStr, RemoveAllContextMenuItems(app_frame));

  const extensions::MenuItem::ExtensionKey extension_key(
      /*extension_id=*/"", web_view_guest->owner_rfh()->GetProcess()->GetID(),
      web_view_guest->owner_rfh()->GetRoutingID(),
      web_view_guest->view_instance_id());
  ASSERT_EQ(0u, menu_manager->MenuItemsSize(extension_key));
}

// This test checks if the Controlled Frame is able to intercept URL navigation
// requests.
IN_PROC_BROWSER_TEST_F(ControlledFrameApiTest, URLLoaderIsProxied) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  const GURL& kOriginalControlledFrameUrl =
      embedded_https_test_server().GetURL("/index.html");
  ASSERT_TRUE(CreateControlledFrame(app_frame, kOriginalControlledFrameUrl));

  auto* web_request_event_router =
      extensions::WebRequestEventRouter::Get(profile());
  EXPECT_EQ(0u, web_request_event_router->GetListenerCountForTesting(
                    profile(), kWebRequestOnBeforeRequestEventName));

  const std::string& kServerHostPort =
      embedded_https_test_server().host_port_pair().ToString();
  EXPECT_EQ("SUCCESS",
            content::EvalJs(app_frame, content::JsReplace(R"(
    (function() {
      const frame = document.getElementsByTagName('controlledframe')[0];
      if (!frame || !frame.request) {
        return 'FAIL: frame or frame.request is undefined';
      }
      frame.request.onBeforeRequest.addListener(() => {
        return { cancel: true };
      }, { urls: ['https://*/controlled_frame_cancel.html'] }, ['blocking']);
      frame.request.onBeforeRequest.addListener(() => {
        return { cancel: false };
      }, { urls: ['https://*/controlled_frame_success.html'] }, ['blocking']);
      frame.request.onBeforeRequest.addListener(() => {
        return {
          redirectUrl: 'https://' + $1 + '/controlled_frame_redirect_target.html'
        };
      }, { urls: ['https://*/controlled_frame_redirect.html'] }, ['blocking']);
      return 'SUCCESS';
    })();
  )",
                                                          kServerHostPort)));
  EXPECT_EQ(3u, web_request_event_router->GetListenerCountForTesting(
                    profile(), kWebRequestOnBeforeRequestEventName));

  auto* web_view_guest = GetWebViewGuest(app_frame);
  content::WebContents* guest_web_contents = web_view_guest->web_contents();

  // Check that navigations can be cancelled.
  {
    content::TestNavigationObserver navigation_observer(
        guest_web_contents, net::Error::ERR_BLOCKED_BY_CLIENT,
        content::MessageLoopRunner::QuitMode::IMMEDIATE,
        /*ignore_uncommitted_navigations=*/false);
    web_view_guest->NavigateGuest(embedded_https_test_server()
                                      .GetURL("/controlled_frame_cancel.html")
                                      .spec(),
                                  /*navigation_handle_callback=*/{},
                                  /*force_navigation=*/false);
    navigation_observer.WaitForNavigationFinished();
    EXPECT_EQ(net::Error::ERR_BLOCKED_BY_CLIENT,
              navigation_observer.last_net_error_code());
    EXPECT_EQ(kOriginalControlledFrameUrl,
              web_view_guest->GetGuestMainFrame()->GetLastCommittedURL());
    EXPECT_FALSE(navigation_observer.last_navigation_succeeded());
  }

  // Check that navigations can be redirected.
  {
    content::TestNavigationObserver navigation_observer(
        guest_web_contents, /*expected_number_of_navigations=*/1u);
    web_view_guest->NavigateGuest(embedded_https_test_server()
                                      .GetURL("/controlled_frame_redirect.html")
                                      .spec(),
                                  /*navigation_handle_callback=*/{},
                                  /*force_navigation=*/false);
    navigation_observer.WaitForNavigationFinished();
    EXPECT_EQ(embedded_https_test_server().GetURL(
                  "/controlled_frame_redirect_target.html"),
              web_view_guest->GetGuestMainFrame()->GetLastCommittedURL());
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
  }

  // Check that navigations can succeed.
  {
    content::TestNavigationObserver navigation_observer(
        guest_web_contents, /*expected_number_of_navigations=*/1u);
    const GURL& kControlledFrameSuccessUrl =
        embedded_https_test_server().GetURL("/controlled_frame_success.html");
    web_view_guest->NavigateGuest(kControlledFrameSuccessUrl.spec(),
                                  /*navigation_handle_callback=*/{},
                                  /*force_navigation=*/false);
    navigation_observer.WaitForNavigationFinished();
    EXPECT_EQ(kControlledFrameSuccessUrl,
              web_view_guest->GetGuestMainFrame()->GetLastCommittedURL());
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
  }
}

IN_PROC_BROWSER_TEST_F(ControlledFrameApiTest, AuthRequestIsProxied) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL("/index.html")));

  auto* web_request_event_router =
      extensions::WebRequestEventRouter::Get(profile());
  EXPECT_EQ(0u, web_request_event_router->GetListenerCountForTesting(
                    profile(), kWebRequestOnAuthRequiredEventName));

  EXPECT_EQ(true, content::EvalJs(app_frame, R"(
    (function() {
      const frame = document.getElementsByTagName('controlledframe')[0];
      if (!frame || !frame.request) {
        return false;
      }

      const expectedUsername = 'test';
      const expectedPassword = 'pass';
      frame.request.onAuthRequired.addListener(() => {
        return {
          authCredentials: {
            username: expectedUsername,
            password: expectedPassword
          }
        };
      }, { urls: [`https://*/auth-basic*`] }, ['blocking']);
      return true;
    })();
  )"));
  EXPECT_EQ(1u, web_request_event_router->GetListenerCountForTesting(
                    profile(), kWebRequestOnAuthRequiredEventName));

  auto* web_view_guest = GetWebViewGuest(app_frame);
  content::WebContents* guest_web_contents = web_view_guest->web_contents();

  // Check that the injecting the credentials through WebRequest produces a
  // successful navigation.
  {
    content::TestNavigationObserver navigation_observer(
        guest_web_contents,
        /*expected_number_of_navigations=*/1u);
    const GURL& kAuthBasicUrl =
        embedded_https_test_server().GetURL("/auth-basic?password=pass");
    web_view_guest->NavigateGuest(kAuthBasicUrl.spec(),
                                  /*navigation_handle_callback=*/{},
                                  /*force_navigation=*/false);
    navigation_observer.WaitForNavigationFinished();
    EXPECT_EQ(kAuthBasicUrl,
              web_view_guest->GetGuestMainFrame()->GetLastCommittedURL());
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
  }

  // Check that the injecting the wrong credentials through WebRequest produces
  // an error.
  {
    content::TestNavigationObserver navigation_observer(
        guest_web_contents,
        /*expected_number_of_navigations=*/1u);
    const GURL& kAuthBasicUrl =
        embedded_https_test_server().GetURL("/auth-basic?password=badpass");
    web_view_guest->NavigateGuest(kAuthBasicUrl.spec(),
                                  /*navigation_handle_callback=*/{},
                                  /*force_navigation=*/false);
    navigation_observer.WaitForNavigationFinished();
    EXPECT_EQ(kAuthBasicUrl,
              web_view_guest->GetGuestMainFrame()->GetLastCommittedURL());
    // The auth request fails but keeps retrying until this error is produced.
    // TODO(crbug.com/40942953): The error produced here should be
    // authentication related.
    EXPECT_EQ(net::Error::ERR_TOO_MANY_RETRIES,
              navigation_observer.last_net_error_code());
    EXPECT_FALSE(navigation_observer.last_navigation_succeeded());
  }
}

IN_PROC_BROWSER_TEST_F(ControlledFrameApiTest, ExecuteScript) {
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app =
      web_app::IsolatedWebAppBuilder(web_app::ManifestBuilder())
          .AddHtml("/execute_script.input.js",
                   "document.body.style.backgroundColor = 'red';")
          .BuildBundle();
  app->TrustSigningKey();
  ASSERT_OK_AND_ASSIGN(web_app::IsolatedWebAppUrlInfo url_info,
                       app->Install(profile()));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL("/index.html")));

  auto* web_view_guest = GetWebViewGuest(app_frame);

  // Verify that executeScript() using JS code can change the background color.
  EXPECT_EQ(kEvalSuccessStr, SetBackgroundColorToWhite(web_view_guest));
  EXPECT_EQ(kEvalSuccessStr, ExecuteScriptRedBackgroundCode(app_frame));
  EXPECT_EQ(kEvalSuccessStr, VerifyBackgroundColorIsRed(web_view_guest));

  // Verify that executeScript() using a JS file changes the background color.
  EXPECT_EQ(kEvalSuccessStr, SetBackgroundColorToWhite(web_view_guest));
  EXPECT_EQ(kEvalSuccessStr, ExecuteScriptRedBackgroundFile(app_frame));
  EXPECT_EQ(kEvalSuccessStr, VerifyBackgroundColorIsRed(web_view_guest));
}

IN_PROC_BROWSER_TEST_F(ControlledFrameApiTest, DisabledInDataIframe) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  GURL https_url = embedded_https_test_server().GetURL("/index.html");
  ASSERT_TRUE(CreateControlledFrame(app_frame, https_url));

  ASSERT_TRUE(ExecJs(app_frame, R"(
      const src = '<!DOCTYPE html><p>data: URL</p>';
      const url = `data:text/html;base64,${btoa(src)}`;
      new Promise(resolve => {
        const f = document.createElement('iframe');
        f.src = url;
        f.addEventListener('load', resolve);
        document.body.appendChild(f);
      });
  )"));
  content::RenderFrameHost* iframe = ChildFrameAt(app_frame, 1);
  ASSERT_NE(iframe, nullptr);

  ASSERT_FALSE(CreateControlledFrame(iframe, https_url));
}

IN_PROC_BROWSER_TEST_F(ControlledFrameApiTest, DisabledInSandboxedIframe) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  GURL https_url = embedded_https_test_server().GetURL("/index.html");
  ASSERT_TRUE(CreateControlledFrame(app_frame, https_url));

  ASSERT_TRUE(
      ExecJs(app_frame, content::JsReplace(R"(
      new Promise(resolve => {
        const f = document.createElement('iframe');
        f.src = $1;
        f.sandbox = 'allow-scripts';  // for EvalJs
        f.addEventListener('load', resolve);
        document.body.appendChild(f);
      });
  )",
                                           url_info.origin().Serialize())));
  content::RenderFrameHost* iframe = ChildFrameAt(app_frame, 1);
  ASSERT_NE(iframe, nullptr);

  ASSERT_FALSE(CreateControlledFrame(iframe, https_url));
}

IN_PROC_BROWSER_TEST_F(ControlledFrameApiTest, DisabledInSrcdocIframe) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(ExecJs(app_frame, R"(
      const noopPolicy = trustedTypes.createPolicy("policy", {
        createHTML: (string) => string,
      });
      new Promise(resolve => {
        const f = document.createElement('iframe');
        f.srcdoc = noopPolicy.createHTML('<!DOCTYPE html><p>srcdoc iframe</p>');
        f.addEventListener('load', resolve);
        document.body.appendChild(f);
      });
  )"));
  content::RenderFrameHost* iframe = ChildFrameAt(app_frame, 0);
  ASSERT_NE(iframe, nullptr);

  // Despite srcdoc iframes being same-origin, creating the <controlledframe>
  // fails because AvailabilityCheck looks at the frame's scheme as well as
  // its isolation level. No other IsolatedContext API does this, but it makes
  // sense for <controlledframe> because it's not a purely JS-based API that
  // will be blocked through CSP.
  ASSERT_FALSE(CreateControlledFrame(
      iframe, embedded_https_test_server().GetURL("/index.html")));
}

IN_PROC_BROWSER_TEST_F(ControlledFrameApiTest, DisabledInBlobIframe) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(ExecJs(app_frame, R"(
      const blob = new Blob(['<!DOCTYPE html><p>blob html page</p>'], {
        type: 'text/html'
      });
      const url = URL.createObjectURL(blob);
      new Promise(resolve => {
        const f = document.createElement('iframe');
        f.src = url;
        f.addEventListener('load', resolve);
        document.body.appendChild(f);
      });
  )"));
  content::RenderFrameHost* iframe = ChildFrameAt(app_frame, 0);
  ASSERT_NE(iframe, nullptr);

  // As with srcdoc iframes, is blocked due to AvailabilityCheck verifying
  // the frame's scheme as well as its isolation level.
  ASSERT_FALSE(CreateControlledFrame(
      iframe, embedded_https_test_server().GetURL("/index.html")));
}

class ControlledFrameWebSocketApiTest : public ControlledFrameApiTest {
 public:
  void SetUpOnMainThread() override {
    ControlledFrameApiTest::SetUpOnMainThread();
    websocket_test_server_ = std::make_unique<net::SpawnedTestServer>(
        net::SpawnedTestServer::TYPE_WS, net::GetWebSocketTestDataDirectory());
    ASSERT_TRUE(websocket_test_server_->Start());
  }

  net::SpawnedTestServer* websocket_test_server() {
    return websocket_test_server_.get();
  }

  GURL GetWebSocketUrl(const std::string& path) {
    GURL::Replacements replacements;
    replacements.SetSchemeStr("ws");
    return websocket_test_server_->GetURL(path).ReplaceComponents(replacements);
  }

 private:
  std::unique_ptr<net::SpawnedTestServer> websocket_test_server_;
};

IN_PROC_BROWSER_TEST_F(ControlledFrameWebSocketApiTest, WebSocketIsProxied) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  const GURL& kOriginalControlledFrameUrl =
      embedded_https_test_server().GetURL("/index.html");
  ASSERT_TRUE(CreateControlledFrame(app_frame, kOriginalControlledFrameUrl));

  auto* web_request_event_router =
      extensions::WebRequestEventRouter::Get(profile());
  EXPECT_EQ(0u, web_request_event_router->GetListenerCountForTesting(
                    profile(), kWebRequestOnBeforeRequestEventName));

  // Use Web Sockets before installing a WebRequest event listener to verify
  // that it works inside of the Controlled Frame.
  auto* web_view_guest = GetWebViewGuest(app_frame);
  content::WebContents* guest_web_contents = web_view_guest->web_contents();
  GURL::Replacements http_scheme_replacement;
  http_scheme_replacement.SetSchemeStr("http");
  const GURL& kWebSocketConnectCheckUrl =
      websocket_test_server()
          ->GetURL("/connect_check.html")
          .ReplaceComponents(http_scheme_replacement);
  {
    content::TitleWatcher title_watcher(guest_web_contents, u"PASS");
    title_watcher.AlsoWaitForTitle(u"FAIL");
    content::TestNavigationObserver navigation_observer(
        guest_web_contents,
        /*expected_number_of_navigations=*/1u);
    web_view_guest->NavigateGuest(kWebSocketConnectCheckUrl.spec(),
                                  /*navigation_handle_callback=*/{},
                                  /*force_navigation=*/false);
    navigation_observer.WaitForNavigationFinished();
    EXPECT_EQ(kWebSocketConnectCheckUrl,
              web_view_guest->GetGuestMainFrame()->GetLastCommittedURL());
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
    EXPECT_EQ(u"PASS", title_watcher.WaitAndGetTitle());
  }

  {
    content::TestNavigationObserver navigation_observer(
        guest_web_contents,
        /*expected_number_of_navigations=*/1u);
    web_view_guest->NavigateGuest(kOriginalControlledFrameUrl.spec(),
                                  /*navigation_handle_callback=*/{},
                                  /*force_navigation=*/false);
    navigation_observer.WaitForNavigationFinished();
    EXPECT_EQ(kOriginalControlledFrameUrl,
              web_view_guest->GetGuestMainFrame()->GetLastCommittedURL());
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
  }

  // Set up a WebRequest event listener that cancels any requests to the Web
  // Socket server.
  EXPECT_EQ(true, content::EvalJs(app_frame,
                                  R"(
    (function() {
      const frame = document.getElementsByTagName('controlledframe')[0];
      if (!frame || !frame.request) {
        return false;
      }
      frame.request.onBeforeRequest.addListener(() => {
        return { cancel: true };
      }, { urls: ['ws://*/*'] }, ['blocking']);
      return true;
    })();
  )"));
  EXPECT_EQ(1u, web_request_event_router->GetListenerCountForTesting(
                    profile(), kWebRequestOnBeforeRequestEventName));
  {
    content::TitleWatcher title_watcher(guest_web_contents, u"PASS");
    title_watcher.AlsoWaitForTitle(u"FAIL");
    content::TestNavigationObserver navigation_observer(
        guest_web_contents,
        /*expected_number_of_navigations=*/1u);
    web_view_guest->NavigateGuest(kWebSocketConnectCheckUrl.spec(),
                                  /*navigation_handle_callback=*/{},
                                  /*force_navigation=*/false);
    navigation_observer.WaitForNavigationFinished();
    EXPECT_EQ(kWebSocketConnectCheckUrl,
              web_view_guest->GetGuestMainFrame()->GetLastCommittedURL());
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
    EXPECT_EQ(u"FAIL", title_watcher.WaitAndGetTitle());
  }
}

class ControlledFrameWebTransportApiTest : public ControlledFrameApiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ControlledFrameApiTest::SetUpCommandLine(command_line);
    webtransport_server_.SetUpCommandLine(command_line);
    webtransport_server_.Start();
  }

  content::WebTransportSimpleTestServer& webtransport_server() {
    return webtransport_server_;
  }

 protected:
  content::WebTransportSimpleTestServer webtransport_server_;
};

IN_PROC_BROWSER_TEST_F(ControlledFrameWebTransportApiTest,
                       WebTransportIsProxied) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL("/index.html")));

  auto* web_request_event_router =
      extensions::WebRequestEventRouter::Get(profile());
  EXPECT_EQ(0u, web_request_event_router->GetListenerCountForTesting(
                    profile(), kWebRequestOnBeforeRequestEventName));

  // Use WebTransport before installing a WebRequest event listener to verify
  // that it works inside of the Controlled Frame.
  auto* web_view_guest = GetWebViewGuest(app_frame);
  EXPECT_EQ(true, content::EvalJs(
                      web_view_guest->GetGuestMainFrame(),
                      content::JsReplace(
                          R"(
    (async function() {
      const url = 'https://localhost:' + $1 + '/echo_test';
      try {
        const transport = new WebTransport(url);
        await transport.ready;
      } catch (e) {
        console.log(url + ': ' + e.name + ': ' + e.message);
        return false;
      }
      return true;
    })();
  )",
                          webtransport_server().server_address().port())));

  // Set up a WebRequest event listener that cancels any requests to the
  // WebTransport server.
  EXPECT_EQ(true, content::EvalJs(app_frame,
                                  R"(
    let cancelRequest = false;
    (function() {
      const frame = document.getElementsByTagName('controlledframe')[0];
      if (!frame || !frame.request) {
        return false;
      }
      const onBeforeRequestHandler =
      frame.request.onBeforeRequest.addListener(() => {
        return { cancel: true };
      }, { urls: ['https://localhost/*'] }, ['blocking']);
      return true;
    })();
  )"));
  EXPECT_EQ(1u, web_request_event_router->GetListenerCountForTesting(
                    profile(), kWebRequestOnBeforeRequestEventName));

  EXPECT_EQ(false, content::EvalJs(
                       web_view_guest->GetGuestMainFrame(),
                       content::JsReplace(
                           R"(
    (async function() {
      cancelRequest = true;
      const url = 'https://localhost:' + $1 + '/echo_test';
      try {
        const transport = new WebTransport(url);
        await transport.ready;
      } catch (e) {
        console.log(url + ': ' + e.name + ': ' + e.message);
        return false;
      }
      return true;
    })();
  )",
                           webtransport_server().server_address().port())));
}

namespace {
constexpr char kPermissionAllowedHost[] = "permission-allowed.com";
constexpr char kPermissionDisallowedHost[] = "permission-disllowed.com";
}  // namespace

class ControlledFramePermissionsPolicyTest : public ControlledFrameApiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ControlledFrameApiTest::SetUpCommandLine(command_line);
    command_line->AppendArg("--use-fake-device-for-media-stream");
  }

  void SetUpPermissionRequestEventListener(content::RenderFrameHost* app_frame,
                                           bool allow_permission) {
    const std::string& handle_request_str = allow_permission ? "allow" : "deny";
    EXPECT_EQ("SUCCESS", content::EvalJs(app_frame, content::JsReplace(
                                                        R"(
      (function() {
        const frame = document.getElementsByTagName('controlledframe')[0];
        if (!frame) {
          return 'FAIL: Could not find a controlledframe element.';
        }
        frame.addEventListener('permissionrequest', (e) => {
          e.request[$1]();
        });
        return 'SUCCESS'
      })();
    )",
                                                        handle_request_str)));
  }

  void RequestMediaPermissionFromControlledFrame(
      content::RenderFrameHost* app_frame,
      bool request_audio,
      bool request_video,
      bool expect_audio_permission_allowed,
      bool expect_video_permission_allowed) {
    extensions::WebViewGuest* web_view_guest = GetWebViewGuest(app_frame);
    EXPECT_EQ("SUCCESS", content::EvalJs(web_view_guest->GetGuestMainFrame(),
                                         content::JsReplace(
                                             R"(
    (async function() {
      const constraints = { audio: $1, video: $2 };
      const expectAudioPermissionAllowed = $3;
      const expectVideoPermissionAllowed = $4;
      try {
        const stream = await navigator.mediaDevices.getUserMedia(constraints);

        const checkPermissionType =
            function(type, tracks, expectPermissionAllowed) {
          const hasTracks = tracks.length;
          if (expectPermissionAllowed != hasTracks) {
            const expectedPermissionStr =
                expectPermissionAllowed ? 'has' : 'does not have';
            const hasTrackStr = hasTracks ? 'has' : 'does not have';
            return 'FAIL: getUserMedia() ' + expectedPermissionStr + ' ' +
                type + ' stream permission, but ' + hasTrackStr + ' ' +
                type + ' tracks';
          }
          return 'SUCCESS';
        }

        let audioPermissionCheckResult = checkPermissionType(
            'audio', stream.getAudioTracks(), expectAudioPermissionAllowed);
        if (audioPermissionCheckResult != 'SUCCESS') {
          return audioPermissionCheckResult;
        }

        let videoPermissionCheckResult = checkPermissionType(
            'video', stream.getVideoTracks(), expectVideoPermissionAllowed);
        if (videoPermissionCheckResult != 'SUCCESS') {
          return videoPermissionCheckResult;
        }

        return 'SUCCESS';
      } catch (err) {
        if (!expectAudioPermissionAllowed && !expectVideoPermissionAllowed) {
          return 'SUCCESS';
        }
        return 'FAIL: ' + err.name + ': ' + err.message;
      }
    })();
  )",
                                             request_audio, request_video,
                                             expect_audio_permission_allowed,
                                             expect_video_permission_allowed)));
  }
};

IN_PROC_BROWSER_TEST_F(ControlledFramePermissionsPolicyTest,
                       CameraPermissionAllowed) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder().AddPermissionsPolicy(
          blink::mojom::PermissionsPolicyFeature::kCamera, /*self=*/true,
          {embedded_https_test_server().GetOrigin(kPermissionAllowedHost)}));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL(kPermissionAllowedHost,
                                                     "/index.html")));

  SetUpPermissionRequestEventListener(app_frame, /*allow_permission=*/true);
  RequestMediaPermissionFromControlledFrame(
      app_frame,
      /*request_audio=*/false,
      /*request_video=*/true,
      /*expect_audio_permission_allowed=*/false,
      /*expect_video_permission_allowed=*/true);
}

IN_PROC_BROWSER_TEST_F(ControlledFramePermissionsPolicyTest,
                       OnlyCameraPermissionAllowed) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder().AddPermissionsPolicy(
          blink::mojom::PermissionsPolicyFeature::kCamera, /*self=*/true,
          {embedded_https_test_server().GetOrigin(kPermissionAllowedHost)}));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL(kPermissionAllowedHost,
                                                     "/index.html")));

  SetUpPermissionRequestEventListener(app_frame, /*allow_permission=*/true);
  RequestMediaPermissionFromControlledFrame(
      app_frame,
      /*request_audio=*/true,
      /*request_video=*/true,
      /*expect_audio_permission_allowed=*/false,
      /*expect_video_permission_allowed=*/true);
}

IN_PROC_BROWSER_TEST_F(ControlledFramePermissionsPolicyTest,
                       CameraPermissionDenied) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder().AddPermissionsPolicy(
          blink::mojom::PermissionsPolicyFeature::kCamera, /*self=*/true,
          {embedded_https_test_server().GetOrigin(kPermissionAllowedHost)}));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL(kPermissionAllowedHost,
                                                     "/index.html")));

  SetUpPermissionRequestEventListener(app_frame, /*allow_permission=*/false);
  RequestMediaPermissionFromControlledFrame(
      app_frame,
      /*request_audio=*/false,
      /*request_video=*/true,
      /*expect_audio_permission_allowed=*/false,
      /*expect_video_permission_allowed=*/false);
}

IN_PROC_BROWSER_TEST_F(ControlledFramePermissionsPolicyTest,
                       CameraPermissionDisallowed) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder().AddPermissionsPolicy(
          blink::mojom::PermissionsPolicyFeature::kCamera, /*self=*/true,
          {embedded_https_test_server().GetOrigin(kPermissionAllowedHost)}));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL(kPermissionDisallowedHost,
                                                     "/index.html")));

  SetUpPermissionRequestEventListener(app_frame, /*allow_permission=*/true);
  RequestMediaPermissionFromControlledFrame(
      app_frame,
      /*request_audio=*/false,
      /*request_video=*/true,
      /*expect_audio_permission_allowed=*/false,
      /*expect_video_permission_allowed=*/false);
}

IN_PROC_BROWSER_TEST_F(ControlledFramePermissionsPolicyTest,
                       MicrophonePermissionAllowed) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder().AddPermissionsPolicy(
          blink::mojom::PermissionsPolicyFeature::kMicrophone, /*self=*/true,
          {embedded_https_test_server().GetOrigin(kPermissionAllowedHost)}));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL(kPermissionAllowedHost,
                                                     "/index.html")));

  SetUpPermissionRequestEventListener(app_frame, /*allow_permission=*/true);
  RequestMediaPermissionFromControlledFrame(
      app_frame,
      /*request_audio=*/true,
      /*request_video=*/false,
      /*expect_audio_permission_allowed=*/true,
      /*expect_video_permission_allowed=*/false);
}

IN_PROC_BROWSER_TEST_F(ControlledFramePermissionsPolicyTest,
                       OnlyMicrophonePermissionAllowed) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder().AddPermissionsPolicy(
          blink::mojom::PermissionsPolicyFeature::kMicrophone, /*self=*/true,
          {embedded_https_test_server().GetOrigin(kPermissionAllowedHost)}));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL(kPermissionAllowedHost,
                                                     "/index.html")));

  SetUpPermissionRequestEventListener(app_frame, /*allow_permission=*/true);
  RequestMediaPermissionFromControlledFrame(
      app_frame,
      /*request_audio=*/true,
      /*request_video=*/true,
      /*expect_audio_permission_allowed=*/true,
      /*expect_video_permission_allowed=*/false);
}

IN_PROC_BROWSER_TEST_F(ControlledFramePermissionsPolicyTest,
                       MicrophonePermissionDenied) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder().AddPermissionsPolicy(
          blink::mojom::PermissionsPolicyFeature::kMicrophone, /*self=*/true,
          {embedded_https_test_server().GetOrigin(kPermissionAllowedHost)}));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL(kPermissionAllowedHost,
                                                     "/index.html")));

  SetUpPermissionRequestEventListener(app_frame, /*allow_permission=*/false);
  RequestMediaPermissionFromControlledFrame(
      app_frame,
      /*request_audio=*/true,
      /*request_video=*/false,
      /*expect_audio_permission_allowed=*/false,
      /*expect_video_permission_allowed=*/false);
}

IN_PROC_BROWSER_TEST_F(ControlledFramePermissionsPolicyTest,
                       MicrophonePermissionDisallowed) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder().AddPermissionsPolicy(
          blink::mojom::PermissionsPolicyFeature::kMicrophone, /*self=*/true,
          {embedded_https_test_server().GetOrigin(kPermissionAllowedHost)}));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL(kPermissionDisallowedHost,
                                                     "/index.html")));

  SetUpPermissionRequestEventListener(app_frame, /*allow_permission=*/true);
  RequestMediaPermissionFromControlledFrame(
      app_frame,
      /*request_audio=*/true,
      /*request_video=*/false,
      /*expect_audio_permission_allowed=*/false,
      /*expect_video_permission_allowed=*/false);
}

IN_PROC_BROWSER_TEST_F(ControlledFramePermissionsPolicyTest,
                       CameraAndMicrophonePermissionAllowed) {
  web_app::IsolatedWebAppUrlInfo url_info = CreateAndInstallEmptyApp(
      web_app::ManifestBuilder()
          .AddPermissionsPolicy(
              blink::mojom::PermissionsPolicyFeature::kCamera, /*self=*/true,
              {embedded_https_test_server().GetOrigin(kPermissionAllowedHost)})
          .AddPermissionsPolicy(
              blink::mojom::PermissionsPolicyFeature::kMicrophone,
              /*self=*/true,
              {embedded_https_test_server().GetOrigin(
                  kPermissionAllowedHost)}));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL(kPermissionAllowedHost,
                                                     "/index.html")));

  SetUpPermissionRequestEventListener(app_frame, /*allow_permission=*/true);
  RequestMediaPermissionFromControlledFrame(
      app_frame,
      /*request_audio=*/true,
      /*request_video=*/true,
      /*expect_audio_permission_allowed=*/true,
      /*expect_video_permission_allowed=*/true);
}

IN_PROC_BROWSER_TEST_F(ControlledFramePermissionsPolicyTest,
                       CameraAndMicrophonePermissionDenied) {
  web_app::IsolatedWebAppUrlInfo url_info = CreateAndInstallEmptyApp(
      web_app::ManifestBuilder()
          .AddPermissionsPolicy(
              blink::mojom::PermissionsPolicyFeature::kCamera, /*self=*/true,
              {embedded_https_test_server().GetOrigin(kPermissionAllowedHost)})
          .AddPermissionsPolicy(
              blink::mojom::PermissionsPolicyFeature::kMicrophone,
              /*self=*/true,
              {embedded_https_test_server().GetOrigin(
                  kPermissionAllowedHost)}));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL(kPermissionAllowedHost,
                                                     "/index.html")));

  SetUpPermissionRequestEventListener(app_frame, /*allow_permission=*/false);
  RequestMediaPermissionFromControlledFrame(
      app_frame,
      /*request_audio=*/true,
      /*request_video=*/true,
      /*expect_audio_permission_allowed=*/false,
      /*expect_video_permission_allowed=*/false);
}

IN_PROC_BROWSER_TEST_F(ControlledFramePermissionsPolicyTest,
                       CameraAndMicrophonePermissionDisallowed) {
  web_app::IsolatedWebAppUrlInfo url_info = CreateAndInstallEmptyApp(
      web_app::ManifestBuilder()
          .AddPermissionsPolicy(
              blink::mojom::PermissionsPolicyFeature::kCamera, /*self=*/true,
              {embedded_https_test_server().GetOrigin(kPermissionAllowedHost)})
          .AddPermissionsPolicy(
              blink::mojom::PermissionsPolicyFeature::kMicrophone,
              /*self=*/true,
              {embedded_https_test_server().GetOrigin(
                  kPermissionAllowedHost)}));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL(kPermissionDisallowedHost,
                                                     "/index.html")));

  SetUpPermissionRequestEventListener(app_frame, /*allow_permission=*/true);
  RequestMediaPermissionFromControlledFrame(
      app_frame,
      /*request_audio=*/true,
      /*request_video=*/true,
      /*expect_audio_permission_allowed=*/false,
      /*expect_video_permission_allowed=*/false);
}

class ControlledFramePromiseApiTest
    : public ControlledFrameApiTest,
      public testing::WithParamInterface<const char*> {};

IN_PROC_BROWSER_TEST_P(ControlledFramePromiseApiTest, PromiseAPIs) {
  std::unique_ptr<web_app::ScopedProxyIsolatedWebApp> app =
      web_app::IsolatedWebAppBuilder(web_app::ManifestBuilder())
          .AddFolderFromDisk("/", "web_apps/simple_isolated_app")
          .BuildAndStartProxyServer();
  ASSERT_OK_AND_ASSIGN(web_app::IsolatedWebAppUrlInfo url_info,
                       app->Install(profile()));
  content::RenderFrameHost* app_frame =
      OpenApp(url_info.app_id(), "/controlled_frame_api_test.html");

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, app->proxy_server().GetURL("/controlled_frame.html")));

  EXPECT_EQ("SUCCESS",
            content::EvalJs(app_frame, content::JsReplace(R"(
        const frame = document.getElementsByTagName('controlledframe')[0];
        testAPI(frame, $1);
    )",
                                                          GetParam())));
}

INSTANTIATE_TEST_SUITE_P(PromiseAPIs,
                         ControlledFramePromiseApiTest,
                         testing::ValuesIn(kControlledFramePromiseApiMethods));

class ControlledFrameServiceWorkerTest
    : public extensions::ServiceWorkerBasedBackgroundTest {
 public:
  ControlledFrameServiceWorkerTest(const ControlledFrameServiceWorkerTest&) =
      delete;
  ControlledFrameServiceWorkerTest& operator=(
      const ControlledFrameServiceWorkerTest&) = delete;

  void SetUpOnMainThread() override {
    extensions::ServiceWorkerBasedBackgroundTest::SetUpOnMainThread();
    embedded_https_test_server().ServeFilesFromSourceDirectory(
        GetChromeTestDataDir());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

 protected:
  ControlledFrameServiceWorkerTest() {
    feature_list.InitWithFeatures(
        /*enabled_features=*/{features::kIsolatedWebApps,
                              features::kIsolatedWebAppDevMode},
        /*disabled_features=*/{});
  }

  ~ControlledFrameServiceWorkerTest() override = default;

  base::test::ScopedFeatureList feature_list;
};

// This test ensures that loading an extension Service Worker does not cause a
// crash, and that Controlled Frame is not allowed in the Service Worker
// context. For more details, see https://crbug.com/1462384.
// This test is the same as ServiceWorkerBasedBackgroundTest.Basic.
IN_PROC_BROWSER_TEST_F(ControlledFrameServiceWorkerTest, PRE_Basic) {
  ExtensionTestMessageListener newtab_listener("CREATED");
  newtab_listener.set_failure_message("CREATE_FAILED");
  ExtensionTestMessageListener worker_listener("WORKER_RUNNING");
  worker_listener.set_failure_message("NON_WORKER_SCOPE");
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(
          "service_worker/worker_based_background/basic"));
  ASSERT_TRUE(extension);
  const extensions::ExtensionId extension_id = extension->id();
  EXPECT_TRUE(worker_listener.WaitUntilSatisfied());

  const GURL url =
      embedded_https_test_server().GetURL("/extensions/test_file.html");
  content::WebContents* new_web_contents =
      extensions::browsertest_util::AddTab(browser(), url);
  EXPECT_TRUE(new_web_contents);
  EXPECT_TRUE(newtab_listener.WaitUntilSatisfied());

  // Service Worker extension does not have ExtensionHost.
  EXPECT_FALSE(process_manager()->GetBackgroundHostForExtension(extension_id));
}

// After browser restarts, this test step ensures that opening a tab fires
// tabs.onCreated event listener to the extension without explicitly loading the
// extension. This is because the extension registered a listener before browser
// restarted in PRE_Basic.
IN_PROC_BROWSER_TEST_F(ControlledFrameServiceWorkerTest, Basic) {
  ExtensionTestMessageListener newtab_listener("CREATED");
  newtab_listener.set_failure_message("CREATE_FAILED");
  const GURL url =
      embedded_https_test_server().GetURL("/extensions/test_file.html");
  content::WebContents* new_web_contents =
      extensions::browsertest_util::AddTab(browser(), url);
  EXPECT_TRUE(new_web_contents);
  EXPECT_TRUE(newtab_listener.WaitUntilSatisfied());
}

class ControlledFrameAvailableChannelTest
    : public ControlledFrameApiTest,
      public testing::WithParamInterface<version_info::Channel> {
 protected:
  ControlledFrameAvailableChannelTest() : channel_(GetParam()) {}

 private:
  extensions::ScopedCurrentChannel channel_;
};

INSTANTIATE_TEST_SUITE_P(ControlledFrameAvailableChannels,
                         ControlledFrameAvailableChannelTest,
                         testing::Values(version_info::Channel::STABLE,
                                         version_info::Channel::BETA,
                                         version_info::Channel::DEV,
                                         version_info::Channel::CANARY,
                                         version_info::Channel::DEFAULT));

IN_PROC_BROWSER_TEST_P(ControlledFrameAvailableChannelTest, Test) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL("/index.html")));

  // Test if Controlled Frame is available.
  EXPECT_EQ(kEvalSuccessStr, ExecuteScriptRedBackgroundFile(app_frame));
}

class ControlledFrameNotAvailableChannelTest
    : public ControlledFrameApiTest,
      public testing::WithParamInterface<version_info::Channel> {
 protected:
  ControlledFrameNotAvailableChannelTest() : channel_(GetParam()) {}

 private:
  extensions::ScopedCurrentChannel channel_;
};

INSTANTIATE_TEST_SUITE_P(ControlledFrameNotAvailableChannels,
                         ControlledFrameNotAvailableChannelTest,
                         testing::Values(version_info::Channel::STABLE,
                                         version_info::Channel::BETA,
                                         version_info::Channel::DEV,
                                         version_info::Channel::CANARY,
                                         version_info::Channel::DEFAULT));

IN_PROC_BROWSER_TEST_P(ControlledFrameNotAvailableChannelTest, Test) {
  // Test if Controlled Frame is not available.
  const GURL start_url("https://app.site.test/example/index");
  InstallPWA(start_url);
  content::WebContents* app_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_FALSE(IsControlledFramePresent(app_contents->GetPrimaryMainFrame()));
}

class ControlledFrameDisabledTest : public ControlledFrameApiTest {
 protected:
  ControlledFrameDisabledTest() {
    feature_list.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kControlledFrame});
  }

  ~ControlledFrameDisabledTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list;
};

IN_PROC_BROWSER_TEST_F(ControlledFrameDisabledTest, MissingFeature) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_FALSE(IsControlledFramePresent(app_frame));
}

}  // namespace controlled_frame
