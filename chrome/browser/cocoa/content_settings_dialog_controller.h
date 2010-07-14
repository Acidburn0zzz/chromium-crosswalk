// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/cocoa_protocols_mac.h"
#include "base/scoped_ptr.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/browser/pref_member.h"

// Index of the "enabled" and "disabled" radio group settings in all tabs except
// for the cookies tab.
const NSInteger kContentSettingsEnabledIndex = 0;
const NSInteger kContentSettingsDisabledIndex = 1;

// Indices of the various cookie settings in the cookie radio group.
const NSInteger kCookieEnabledIndex = 0;
const NSInteger kCookieDisabledIndex = 1;

// Indices of the various geolocation settings in the geolocation radio group.
const NSInteger kGeolocationEnabledIndex = 0;
const NSInteger kGeolocationAskIndex = 1;
const NSInteger kGeolocationDisabledIndex = 2;

// Indices of the various notifications settings in the geolocation radio group.
const NSInteger kNotificationsEnabledIndex = 0;
const NSInteger kNotificationsAskIndex = 1;
const NSInteger kNotificationsDisabledIndex = 2;

namespace ContentSettingsDialogControllerInternal {
class PrefObserverBridge;
}

class Profile;
@class TabViewPickerTable;

// This controller manages a dialog that lets the user manage the content
// settings for several content setting types.
@interface ContentSettingsDialogController
    : NSWindowController<NSWindowDelegate, NSTabViewDelegate> {
 @private
  IBOutlet NSTabView* tabView_;
  IBOutlet TabViewPickerTable* tabViewPicker_;
  Profile* profile_;  // weak
  IntegerPrefMember lastSelectedTab_;
  BooleanPrefMember clearSiteDataOnExit_;
  scoped_ptr<ContentSettingsDialogControllerInternal::PrefObserverBridge>
      observer_;  // Watches for pref changes.
}

// Show the content settings dialog associated with the given profile (or the
// original profile if this is an incognito profile).  If no content settings
// dialog exists for this profile, create one and show it.  Any resulting
// editor releases itself when closed.
+(id)showContentSettingsForType:(ContentSettingsType)settingsType
                        profile:(Profile*)profile;

// Closes an exceptions sheet, if one is attached.
- (void)closeExceptionsSheet;

- (IBAction)showCookies:(id)sender;
- (IBAction)openFlashPlayerSettings:(id)sender;
- (IBAction)openPluginsPage:(id)sender;

- (IBAction)showCookieExceptions:(id)sender;
- (IBAction)showImagesExceptions:(id)sender;
- (IBAction)showJavaScriptExceptions:(id)sender;
- (IBAction)showPluginsExceptions:(id)sender;
- (IBAction)showPopupsExceptions:(id)sender;
- (IBAction)showGeolocationExceptions:(id)sender;
- (IBAction)showNotificationsExceptions:(id)sender;

@end

@interface ContentSettingsDialogController (TestingAPI)
// Properties that the radio groups and checkboxes are bound to.
@property(assign, nonatomic) NSInteger cookieSettingIndex;
@property(assign, nonatomic) BOOL blockThirdPartyCookies;
@property(assign, nonatomic) BOOL clearSiteDataOnExit;
@property(assign, nonatomic) NSInteger imagesEnabledIndex;
@property(assign, nonatomic) NSInteger javaScriptEnabledIndex;
@property(assign, nonatomic) NSInteger popupsEnabledIndex;
@property(assign, nonatomic) NSInteger pluginsEnabledIndex;
@property(assign, nonatomic) NSInteger geolocationSettingIndex;
@property(assign, nonatomic) NSInteger notificationsSettingIndex;
@end
