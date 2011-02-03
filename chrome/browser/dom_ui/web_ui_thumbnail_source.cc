// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/web_ui_thumbnail_source.h"

#include "base/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

WebUIThumbnailSource::WebUIThumbnailSource(Profile* profile)
    : DataSource(chrome::kChromeUIThumbnailPath, MessageLoop::current()),
      // Set TopSites now as Profile isn't thread safe.
      top_sites_(profile->GetTopSites()) {
}

WebUIThumbnailSource::~WebUIThumbnailSource() {
}

void WebUIThumbnailSource::StartDataRequest(const std::string& path,
                                            bool is_off_the_record,
                                            int request_id) {
  scoped_refptr<RefCountedBytes> data;
  if (top_sites_->GetPageThumbnail(GURL(path), &data)) {
    // We have the thumbnail.
    SendResponse(request_id, data.get());
  } else {
    SendDefaultThumbnail(request_id);
  }
}

std::string WebUIThumbnailSource::GetMimeType(const std::string&) const {
  // We need to explicitly return a mime type, otherwise if the user tries to
  // drag the image they get no extension.
  return "image/png";
}

MessageLoop* WebUIThumbnailSource::MessageLoopForRequestPath(
    const std::string& path) const {
  // TopSites can be accessed from the IO thread.
  return top_sites_.get() ? NULL : DataSource::MessageLoopForRequestPath(path);
}

void WebUIThumbnailSource::SendDefaultThumbnail(int request_id) {
  // Use placeholder thumbnail.
  if (!default_thumbnail_.get()) {
    default_thumbnail_ =
        ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
            IDR_DEFAULT_THUMBNAIL);
  }
  SendResponse(request_id, default_thumbnail_);
}
