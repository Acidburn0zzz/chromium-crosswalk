// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/network_menu_icon.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/skbitmap_operations.h"

using std::max;
using std::min;

namespace chromeos {

namespace {

// Amount to fade icons while connecting.
const double kConnectingImageAlpha = 0.5;

// Animation cycle length.
const int kThrobDurationMs = 750;

// Images for strength bars for wired networks.
const int kNumBarsImages = 5;
gfx::ImageSkia* kBarsImagesAnimatingDark[kNumBarsImages - 1];
gfx::ImageSkia* kBarsImagesAnimatingLight[kNumBarsImages - 1];

// Imagaes for strength arcs for wireless networks.
const int kNumArcsImages = 5;
gfx::ImageSkia* kArcsImagesAnimatingDark[kNumArcsImages - 1];
gfx::ImageSkia* kArcsImagesAnimatingLight[kNumArcsImages - 1];

// Badge offsets. The right and bottom offsets are computed based on the size
// of the network icon and the badge in order to accomodate multiple icon
// resolutions (ie. standard and high DPI).
const int kBadgeLeftX = 0;
const int kBadgeTopY = 0;

// ID for VPN badge.
const int kVpnBadgeId = IDR_STATUSBAR_VPN_BADGE;

int StrengthIndex(int strength, int count) {
  if (strength == 0) {
    return 0;
  } else {
    // Return an index in the range [1, count].
    const float findex = (static_cast<float>(strength) / 100.0f) *
        nextafter(static_cast<float>(count), 0);
    int index = 1 + static_cast<int>(findex);
    index = max(min(index, count), 1);
    return index;
  }
}

int WifiStrengthIndex(const WifiNetwork* wifi) {
  return StrengthIndex(wifi->strength(), kNumArcsImages - 1);
}

int WimaxStrengthIndex(const WimaxNetwork* wimax) {
  return StrengthIndex(wimax->strength(), kNumBarsImages - 1);
}

int CellularStrengthIndex(const CellularNetwork* cellular) {
  if (cellular->data_left() == CellularNetwork::DATA_NONE)
    return 0;
  else
    return StrengthIndex(cellular->strength(), kNumBarsImages - 1);
}

const gfx::ImageSkia* BadgeForNetworkTechnology(
    const CellularNetwork* cellular,
    NetworkMenuIcon::ResourceColorTheme color) {
  const int kUnknownBadgeType = -1;
  int id = kUnknownBadgeType;
  switch (cellular->network_technology()) {
    case NETWORK_TECHNOLOGY_EVDO:
      switch (cellular->data_left()) {
        case CellularNetwork::DATA_NONE:
          id = IDR_STATUSBAR_NETWORK_3G_ERROR;
          break;
        case CellularNetwork::DATA_VERY_LOW:
        case CellularNetwork::DATA_LOW:
        case CellularNetwork::DATA_NORMAL:
          id = (color == NetworkMenuIcon::COLOR_DARK) ?
              IDR_STATUSBAR_NETWORK_3G_DARK :
              IDR_STATUSBAR_NETWORK_3G_LIGHT;
          break;
        case CellularNetwork::DATA_UNKNOWN:
          id = IDR_STATUSBAR_NETWORK_3G_UNKNOWN;
          break;
      }
      break;
    case NETWORK_TECHNOLOGY_1XRTT:
      switch (cellular->data_left()) {
        case CellularNetwork::DATA_NONE:
          id = IDR_STATUSBAR_NETWORK_1X_ERROR;
          break;
        case CellularNetwork::DATA_VERY_LOW:
        case CellularNetwork::DATA_LOW:
        case CellularNetwork::DATA_NORMAL:
          id = IDR_STATUSBAR_NETWORK_1X;
          break;
        case CellularNetwork::DATA_UNKNOWN:
          id = IDR_STATUSBAR_NETWORK_1X_UNKNOWN;
          break;
      }
      break;
      // Note: we may not be able to obtain data usage info from GSM carriers,
      // so there may not be a reason to create _ERROR or _UNKNOWN versions
      // of the following icons.
    case NETWORK_TECHNOLOGY_GPRS:
      id = IDR_STATUSBAR_NETWORK_GPRS;
      break;
    case NETWORK_TECHNOLOGY_EDGE:
      id = (color == NetworkMenuIcon::COLOR_DARK) ?
          IDR_STATUSBAR_NETWORK_EDGE_DARK :
          IDR_STATUSBAR_NETWORK_EDGE_LIGHT;
      break;
    case NETWORK_TECHNOLOGY_UMTS:
      id =  (color == NetworkMenuIcon::COLOR_DARK) ?
          IDR_STATUSBAR_NETWORK_3G_DARK :
          IDR_STATUSBAR_NETWORK_3G_LIGHT;
      break;
    case NETWORK_TECHNOLOGY_HSPA:
      id = IDR_STATUSBAR_NETWORK_HSPA;
      break;
    case NETWORK_TECHNOLOGY_HSPA_PLUS:
      id = IDR_STATUSBAR_NETWORK_HSPA_PLUS;
      break;
    case NETWORK_TECHNOLOGY_LTE:
      id = IDR_STATUSBAR_NETWORK_LTE;
      break;
    case NETWORK_TECHNOLOGY_LTE_ADVANCED:
      id = IDR_STATUSBAR_NETWORK_LTE_ADVANCED;
      break;
    case NETWORK_TECHNOLOGY_GSM:
      id = IDR_STATUSBAR_NETWORK_GPRS;
      break;
    case NETWORK_TECHNOLOGY_UNKNOWN:
      break;
  }
  if (id == kUnknownBadgeType)
    return NULL;
  else
    return ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(id);
}

const SkBitmap GetEmptyBitmapOfSameSize(const gfx::ImageSkiaRep& reference) {
  typedef std::pair<int, int> SizeKey;
  typedef std::map<SizeKey, SkBitmap> SizeBitmapMap;
  static SizeBitmapMap* empty_bitmaps_ = new SizeBitmapMap;

  SizeKey key(reference.pixel_width(), reference.pixel_height());

  SizeBitmapMap::iterator iter = empty_bitmaps_->find(key);
  if (iter != empty_bitmaps_->end())
    return iter->second;

  SkBitmap empty;
  empty.setConfig(SkBitmap::kARGB_8888_Config, key.first, key.second);
  empty.allocPixels();
  empty.eraseARGB(0, 0, 0, 0);
  (*empty_bitmaps_)[key] = empty;
  return empty;
}

class FadedImageSource : public gfx::ImageSkiaSource {
 public:
  FadedImageSource(const gfx::ImageSkia& source, double alpha)
      : source_(source),
        alpha_(alpha) {
  }
  virtual ~FadedImageSource() {}

  virtual gfx::ImageSkiaRep GetImageForScale(
      ui::ScaleFactor scale_factor) OVERRIDE {
    gfx::ImageSkiaRep image_rep = source_.GetRepresentation(scale_factor);
    const SkBitmap empty_bitmap = GetEmptyBitmapOfSameSize(image_rep);
    SkBitmap faded_bitmap = SkBitmapOperations::CreateBlendedBitmap(
        empty_bitmap, image_rep.sk_bitmap(), alpha_);
    return gfx::ImageSkiaRep(faded_bitmap, image_rep.scale_factor());
  }

 private:
  const gfx::ImageSkia source_;
  const float alpha_;

  DISALLOW_COPY_AND_ASSIGN(FadedImageSource);
};

// This defines how we assemble a network icon.
class NetworkIconImageSource : public gfx::ImageSkiaSource {
 public:
  NetworkIconImageSource(const gfx::ImageSkia& icon,
                         const gfx::ImageSkia* top_left_badge,
                         const gfx::ImageSkia* top_right_badge,
                         const gfx::ImageSkia* bottom_left_badge,
                         const gfx::ImageSkia* bottom_right_badge)
      : icon_(icon),
        top_left_badge_(top_left_badge),
        top_right_badge_(top_right_badge),
        bottom_left_badge_(bottom_left_badge),
        bottom_right_badge_(bottom_right_badge) {
  }
  virtual ~NetworkIconImageSource() {}

  virtual gfx::ImageSkiaRep GetImageForScale(
      ui::ScaleFactor scale_factor) OVERRIDE {
    gfx::ImageSkiaRep icon_rep = icon_.GetRepresentation(scale_factor);
    if (icon_rep.is_null())
      return gfx::ImageSkiaRep();
    gfx::Canvas canvas(icon_rep, false);
    if (top_left_badge_)
      canvas.DrawImageInt(*top_left_badge_, kBadgeLeftX, kBadgeTopY);
    if (top_right_badge_)
      canvas.DrawImageInt(*top_right_badge_,
                          icon_.width() - top_right_badge_->width(),
                          kBadgeTopY);
    if (bottom_left_badge_) {
      canvas.DrawImageInt(*bottom_left_badge_,
                          kBadgeLeftX,
                          icon_.height() - bottom_left_badge_->height());
    }
    if (bottom_right_badge_) {
      canvas.DrawImageInt(*bottom_right_badge_,
                          icon_.width() - bottom_right_badge_->width(),
                          icon_.height() - bottom_right_badge_->height());
    }
    return canvas.ExtractImageRep();
  }

 private:
  const gfx::ImageSkia icon_;
  const gfx::ImageSkia *top_left_badge_;
  const gfx::ImageSkia *top_right_badge_;
  const gfx::ImageSkia *bottom_left_badge_;
  const gfx::ImageSkia *bottom_right_badge_;

  DISALLOW_COPY_AND_ASSIGN(NetworkIconImageSource);
};

// This defines how we assemble a network menu icon.
class NetworkMenuIconSource : public gfx::ImageSkiaSource {
 public:
  NetworkMenuIconSource(NetworkMenuIcon::ImageType type,
                        int index,
                        NetworkMenuIcon::ResourceColorTheme color)
      : type_(type),
        index_(index),
        color_(color) {
  }
  virtual ~NetworkMenuIconSource() {}

  virtual gfx::ImageSkiaRep GetImageForScale(
      ui::ScaleFactor scale_factor) OVERRIDE {
    int width, height;
    gfx::ImageSkia* images;
    if (type_ == NetworkMenuIcon::ARCS) {
      if (index_ >= kNumArcsImages)
        return gfx::ImageSkiaRep();
      images = ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          color_ == NetworkMenuIcon::COLOR_DARK ?
          IDR_STATUSBAR_NETWORK_ARCS_DARK : IDR_STATUSBAR_NETWORK_ARCS_LIGHT);
      width = images->width();
      height = images->height() / kNumArcsImages;
    } else {
      if (index_ >= kNumBarsImages)
        return gfx::ImageSkiaRep();

      images = ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          color_ == NetworkMenuIcon::COLOR_DARK ?
          IDR_STATUSBAR_NETWORK_BARS_DARK : IDR_STATUSBAR_NETWORK_BARS_LIGHT);
      width = images->width();
      height = images->height() / kNumBarsImages;
    }
    gfx::ImageSkiaRep image_rep = images->GetRepresentation(scale_factor);

    float scale = ui::GetScaleFactorScale(image_rep.scale_factor());
    height *= scale;
    width *= scale;

    SkIRect subset = SkIRect::MakeXYWH(0, index_ * height, width, height);

    SkBitmap dst_bitmap;
    image_rep.sk_bitmap().extractSubset(&dst_bitmap, subset);
    return gfx::ImageSkiaRep(dst_bitmap, image_rep.scale_factor());
  }

  gfx::Size size() const {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    // NeworkMenuIcons all have the same size in DIP for arc/bars.
    if (type_ == NetworkMenuIcon::ARCS) {
      gfx::Size size = rb.GetImageSkiaNamed(
          IDR_STATUSBAR_NETWORK_ARCS_DARK)->size();
      return gfx::Size(size.width(), size.height() / kNumArcsImages);
    } else {
      gfx::Size size = rb.GetImageSkiaNamed(
          IDR_STATUSBAR_NETWORK_BARS_DARK)->size();
      return gfx::Size(size.width(), size.height() / kNumBarsImages);
    }
  }

 private:
  const NetworkMenuIcon::ImageType type_;
  const int index_;
  const NetworkMenuIcon::ResourceColorTheme color_;

  DISALLOW_COPY_AND_ASSIGN(NetworkMenuIconSource);
};

gfx::ImageSkia CreateVpnImage() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const gfx::ImageSkia* ethernet_icon = rb.GetImageSkiaNamed(IDR_STATUSBAR_VPN);
  const gfx::ImageSkia* vpn_badge = rb.GetImageSkiaNamed(kVpnBadgeId);
  return NetworkMenuIcon::GenerateImageFromComponents(
      *ethernet_icon, NULL, NULL, vpn_badge, NULL);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NetworkIcon

// Sets up and generates an ImageSkia for a Network icon.
class NetworkIcon {
 public:
  // Default constructor is used by the status bar icon (NetworkMenuIcon).
  explicit NetworkIcon(NetworkMenuIcon::ResourceColorTheme color);

  // Service path constructor for cached network service icons.
  NetworkIcon(const std::string& service_path,
              NetworkMenuIcon::ResourceColorTheme color);

  ~NetworkIcon();

  // Resets the icon state.
  void ClearIconAndBadges();

  // Resets the saved state to force an update.
  void SetDirty();

  // Determines whether or not the associated network might be dirty and if so
  // updates and generates the icon. Does nothing if network no longer exists.
  void Update();

  // Sets up the base icon image.
  void SetIcon(const Network* network);

  // Sets up the various badges:
  // top_left: cellular roaming
  // top_right: libcros warning
  // bottom_left: VPN
  // bottom_right: disconnected / secure / technology / warning
  void SetBadges(const Network* network);

  // Clears any previous state then sets the base icon and badges.
  void UpdateIcon(const Network* network);

  // Generates the image. Call after setting the icon and badges.
  void GenerateImage();

  const gfx::ImageSkia GetImage() const { return image_; }

  void set_icon(const gfx::ImageSkia& icon) { icon_ = icon; }
  void set_top_left_badge(const gfx::ImageSkia* badge) {
    top_left_badge_ = badge;
  }
  void set_top_right_badge(const gfx::ImageSkia* badge) {
    top_right_badge_ = badge;
  }
  void set_bottom_left_badge(const gfx::ImageSkia* badge) {
    bottom_left_badge_ = badge;
  }
  void set_bottom_right_badge(const gfx::ImageSkia* badge) {
    bottom_right_badge_ = badge;
  }

 private:
  // Updates strength_index_ for wifi or cellular networks.
  // Returns true if |strength_index_| changed.
  bool UpdateWirelessStrengthIndex(const Network* network);

  // Updates the local state for cellular networks.
  bool UpdateCellularState(const Network* network);

  std::string service_path_;
  ConnectionState state_;
  NetworkMenuIcon::ResourceColorTheme resource_color_theme_;
  int strength_index_;
  gfx::ImageSkia image_;
  gfx::ImageSkia icon_;
  const gfx::ImageSkia* top_left_badge_;
  const gfx::ImageSkia* top_right_badge_;
  const gfx::ImageSkia* bottom_left_badge_;
  const gfx::ImageSkia* bottom_right_badge_;
  bool is_status_bar_;
  const Network* connected_network_;  // weak pointer; used for VPN icons.
  NetworkRoamingState roaming_state_;

  DISALLOW_COPY_AND_ASSIGN(NetworkIcon);
};

////////////////////////////////////////////////////////////////////////////////
// NetworkIcon

NetworkIcon::NetworkIcon(NetworkMenuIcon::ResourceColorTheme color)
    : state_(STATE_UNKNOWN),
      resource_color_theme_(color),
      strength_index_(-1),
      top_left_badge_(NULL),
      top_right_badge_(NULL),
      bottom_left_badge_(NULL),
      bottom_right_badge_(NULL),
      is_status_bar_(true),
      connected_network_(NULL),
      roaming_state_(ROAMING_STATE_UNKNOWN) {
}

NetworkIcon::NetworkIcon(const std::string& service_path,
                         NetworkMenuIcon::ResourceColorTheme color)
    : service_path_(service_path),
      state_(STATE_UNKNOWN),
      resource_color_theme_(color),
      strength_index_(-1),
      top_left_badge_(NULL),
      top_right_badge_(NULL),
      bottom_left_badge_(NULL),
      bottom_right_badge_(NULL),
      is_status_bar_(false),
      connected_network_(NULL),
      roaming_state_(ROAMING_STATE_UNKNOWN) {
}

NetworkIcon::~NetworkIcon() {
}

void NetworkIcon::ClearIconAndBadges() {
  icon_ = gfx::ImageSkia();
  top_left_badge_ = NULL;
  top_right_badge_ = NULL;
  bottom_left_badge_ = NULL;
  bottom_right_badge_ = NULL;
}

void NetworkIcon::SetDirty() {
  state_ = STATE_UNKNOWN;
  strength_index_ = -1;
}

void NetworkIcon::Update() {
  chromeos::NetworkLibrary* cros =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  // First look for a visible network.
  const Network* network = cros->FindNetworkByPath(service_path_);
  if (!network) {
    // If not a visible network, check for a remembered network.
    network = cros->FindRememberedNetworkByPath(service_path_);
    if (!network) {
      LOG(WARNING) << "Unable to find network:" << service_path_;
      return;
    }
  }

  // Determine whether or not we need to update the icon.
  bool dirty = image_.empty();

  // If the network state has changed, the icon needs updating.
  if (state_ != network->state()) {
    state_ = network->state();
    dirty = true;
  }

  ConnectionType type = network->type();
  if (type == TYPE_WIFI || type == TYPE_WIMAX || type == TYPE_CELLULAR) {
    if (UpdateWirelessStrengthIndex(network))
      dirty = true;
  }

  if (type == TYPE_CELLULAR) {
    if (UpdateCellularState(network))
      dirty = true;
  }

  if (type == TYPE_VPN) {
    if (cros->connected_network() != connected_network_) {
      connected_network_ = cros->connected_network();
      dirty = true;
    }
  }

  if (dirty) {
    // Set the icon and badges based on the network.
    UpdateIcon(network);
    // Generate the image from the icon.
    GenerateImage();
  }
}

void NetworkIcon::SetIcon(const Network* network) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  switch (network->type()) {
    case TYPE_ETHERNET: {
      icon_ = *rb.GetImageSkiaNamed(IDR_STATUSBAR_WIRED);
      break;
    }
    case TYPE_WIFI: {
      const WifiNetwork* wifi = static_cast<const WifiNetwork*>(network);
      if (strength_index_ == -1)
        strength_index_ = WifiStrengthIndex(wifi);
      icon_ = NetworkMenuIcon::GetImage(
          NetworkMenuIcon::ARCS, strength_index_, resource_color_theme_);
      break;
    }
    case TYPE_WIMAX: {
      const WimaxNetwork* wimax = static_cast<const WimaxNetwork*>(network);
      if (strength_index_ == -1)
        strength_index_ =  WimaxStrengthIndex(wimax);
      icon_ = NetworkMenuIcon::GetImage(
          NetworkMenuIcon::BARS, strength_index_, resource_color_theme_);
      break;
    }
    case TYPE_CELLULAR: {
      const CellularNetwork* cellular =
          static_cast<const CellularNetwork*>(network);
      if (strength_index_ == -1)
        strength_index_ = CellularStrengthIndex(cellular);
      icon_ = NetworkMenuIcon::GetImage(
          NetworkMenuIcon::BARS, strength_index_, resource_color_theme_);
      break;
    }
    case TYPE_VPN: {
      icon_ = *rb.GetImageSkiaNamed(IDR_STATUSBAR_VPN);
      break;
    }
    default: {
      LOG(WARNING) << "Request for icon for unsupported type: "
                   << network->type();
      icon_ = *rb.GetImageSkiaNamed(IDR_STATUSBAR_WIRED);
      break;
    }
  }
}

void NetworkIcon::SetBadges(const Network* network) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  chromeos::NetworkLibrary* cros =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();

  bool use_dark_icons = resource_color_theme_ == NetworkMenuIcon::COLOR_DARK;
  switch (network->type()) {
    case TYPE_ETHERNET: {
      if (network->disconnected()) {
        bottom_right_badge_ =
            rb.GetImageSkiaNamed(IDR_STATUSBAR_NETWORK_DISCONNECTED);
      }
      break;
    }
    case TYPE_WIFI: {
      const WifiNetwork* wifi = static_cast<const WifiNetwork*>(network);
      if (wifi->encrypted() && use_dark_icons) {
        bottom_right_badge_ = rb.GetImageSkiaNamed(
            IDR_STATUSBAR_NETWORK_SECURE_DARK);
      }
      break;
    }
    case TYPE_WIMAX: {
      top_left_badge_ = rb.GetImageSkiaNamed(use_dark_icons ?
          IDR_STATUSBAR_NETWORK_4G_DARK : IDR_STATUSBAR_NETWORK_4G_LIGHT);
      break;
    }
    case TYPE_CELLULAR: {
      const CellularNetwork* cellular =
            static_cast<const CellularNetwork*>(network);
      if (cellular->roaming_state() == ROAMING_STATE_ROAMING &&
          !cros->IsCellularAlwaysInRoaming()) {
        // For cellular that always in roaming don't show roaming badge.
        bottom_right_badge_ = rb.GetImageSkiaNamed(use_dark_icons ?
            IDR_STATUSBAR_NETWORK_ROAMING_DARK :
            IDR_STATUSBAR_NETWORK_ROAMING_LIGHT);
      }
      if (!cellular->connecting()) {
        top_left_badge_ = BadgeForNetworkTechnology(cellular,
                                                    resource_color_theme_);
      }
      break;
    }
    default:
      break;
  }
  // Display the VPN badge too.
  if (resource_color_theme_ == NetworkMenuIcon::COLOR_DARK &&
      cros->virtual_network() &&
      (cros->virtual_network()->connected() ||
       cros->virtual_network()->connecting())) {
    bottom_left_badge_ = rb.GetImageSkiaNamed(kVpnBadgeId);
  }
}

void NetworkIcon::UpdateIcon(const Network* network) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  chromeos::NetworkLibrary* cros =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();

  ClearIconAndBadges();

  if (network->type() != TYPE_VPN) {
    SetIcon(network);
    SetBadges(network);
    return;
  }

  // VPN should never be the primiary active network. This is used for
  // the icon next to a connected or disconnected VPN.
  const Network* connected_network = cros->connected_network();
  if (connected_network && connected_network->type() != TYPE_VPN) {
    // Set the icon and badges for the connected network.
    SetIcon(connected_network);
    SetBadges(connected_network);
  } else {
    // Use the ethernet icon for VPN when not connected.
    icon_ = *rb.GetImageSkiaNamed(IDR_STATUSBAR_WIRED);
    // We can be connected to a VPN, even when there is no connected
    // underlying network. In that case, for the status bar, show the
    // disconnected badge.
    if (is_status_bar_) {
      bottom_right_badge_ =
        rb.GetImageSkiaNamed(IDR_STATUSBAR_NETWORK_DISCONNECTED);
    }
  }
  // Overlay the VPN badge.
  bottom_left_badge_ = rb.GetImageSkiaNamed(kVpnBadgeId);
}

void NetworkIcon::GenerateImage() {
  if (icon_.empty())
    return;

  image_ = NetworkMenuIcon::GenerateImageFromComponents(icon_, top_left_badge_,
      top_right_badge_, bottom_left_badge_, bottom_right_badge_);
}

bool NetworkIcon::UpdateWirelessStrengthIndex(const Network* network) {
  bool dirty = false;
  ConnectionType type = network->type();
  int index = 0;
  if (type == TYPE_WIFI) {
    index = WifiStrengthIndex(static_cast<const WifiNetwork*>(network));
  } else if (type == TYPE_WIMAX) {
    index = WimaxStrengthIndex(static_cast<const WimaxNetwork*>(network));
  } else if (type == TYPE_CELLULAR) {
    index = CellularStrengthIndex(static_cast<const CellularNetwork*>(network));
  }
  if (index != strength_index_) {
    strength_index_ = index;
    dirty = true;
  }
  return dirty;
}

bool NetworkIcon::UpdateCellularState(const Network* network) {
  if (network->type() != TYPE_CELLULAR)
    return false;
  bool dirty = false;
  const CellularNetwork* cellular =
    static_cast<const CellularNetwork*>(network);
  const gfx::ImageSkia* technology_badge = BadgeForNetworkTechnology(
      cellular, resource_color_theme_);
  if (technology_badge != top_left_badge_) {
    dirty = true;
  }
  if (cellular->roaming_state() != roaming_state_) {
    roaming_state_ = cellular->roaming_state();
    dirty = true;
  }
  return dirty;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuIcon

NetworkMenuIcon::NetworkMenuIcon(Delegate* delegate, Mode mode)
    : mode_(mode),
      delegate_(delegate),
      resource_color_theme_(COLOR_DARK),
      ALLOW_THIS_IN_INITIALIZER_LIST(animation_connecting_(this)),
      last_network_type_(TYPE_WIFI),
      connecting_network_(NULL) {
  // Set up the connection animation throbber.
  animation_connecting_.SetThrobDuration(kThrobDurationMs);
  animation_connecting_.SetTweenType(ui::Tween::LINEAR);

  // Initialize the icon.
  icon_.reset(new NetworkIcon(resource_color_theme_));
}

NetworkMenuIcon::~NetworkMenuIcon() {
}

// Public methods:

void NetworkMenuIcon::SetResourceColorTheme(ResourceColorTheme color) {
  if (color == resource_color_theme_)
    return;

  resource_color_theme_ = color;
  icon_.reset(new NetworkIcon(resource_color_theme_));
}

const gfx::ImageSkia NetworkMenuIcon::GetIconAndText(string16* text) {
  SetIconAndText();
  if (text)
    *text = text_;
  icon_->GenerateImage();
  return icon_->GetImage();
}

void NetworkMenuIcon::AnimationProgressed(const ui::Animation* animation) {
  if (animation == &animation_connecting_ && delegate_) {
    // Only update the connecting network from here.
    if (GetConnectingNetwork() == connecting_network_)
      delegate_->NetworkMenuIconChanged();
  }
}

// Private methods:

// In menu mode, returns any connecting network.
// In dropdown mode, only returns connecting network if not connected.
const Network* NetworkMenuIcon::GetConnectingNetwork() {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  if ((mode_ == MENU_MODE) ||
      (mode_ == DROPDOWN_MODE && !cros->connected_network())) {
    const Network* connecting_network = cros->connecting_network();
    // Only show connecting icon for wireless networks.
    if (connecting_network && connecting_network->type() != TYPE_ETHERNET) {
      return connecting_network;
    }
  }
  return NULL;
}

double NetworkMenuIcon::GetAnimation() {
  if (!animation_connecting_.is_animating()) {
    animation_connecting_.Reset();
    animation_connecting_.StartThrobbing(-1 /*throb indefinitely*/);
    return 0;
  }
  return animation_connecting_.GetCurrentValue();
}

// TODO(stevenjb): move below SetIconAndText.
void NetworkMenuIcon::SetConnectingIconAndText() {
  int image_count;
  ImageType image_type;
  gfx::ImageSkia** images;

  if (connecting_network_->type() == TYPE_WIFI) {
    image_count = kNumArcsImages - 1;
    image_type = ARCS;
    images = resource_color_theme_ == COLOR_DARK ? kArcsImagesAnimatingDark :
                                                   kArcsImagesAnimatingLight;
  } else {
    image_count = kNumBarsImages - 1;
    image_type = BARS;
    images = resource_color_theme_ == COLOR_DARK ? kBarsImagesAnimatingDark :
                                                   kBarsImagesAnimatingLight;
  }
  int index = GetAnimation() * nextafter(static_cast<float>(image_count), 0);
  index = std::max(std::min(index, image_count - 1), 0);

  // Lazily cache images.
  if (!images[index]) {
    gfx::ImageSkia source =
        GetImage(image_type, index + 1, resource_color_theme_);
    images[index] =
        new gfx::ImageSkia(NetworkMenuIcon::GenerateConnectingImage(source));
  }
  icon_->set_icon(*images[index]);
  icon_->SetBadges(connecting_network_);
  if (mode_ == MENU_MODE) {
    text_ = l10n_util::GetStringFUTF16(
        IDS_STATUSBAR_NETWORK_CONNECTING_TOOLTIP,
        UTF8ToUTF16(connecting_network_->name()));
  } else {
    text_ = UTF8ToUTF16(connecting_network_->name());
  }
}

// Sets up the icon and badges for GenerateBitmap().
void NetworkMenuIcon::SetIconAndText() {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  DCHECK(cros);

  if (cros->wifi_scanning())
    return;  // Don't update icon while scanning

  icon_->ClearIconAndBadges();

  // If we are connecting to a network, display that.
  connecting_network_ = GetConnectingNetwork();
  if (connecting_network_) {
    SetConnectingIconAndText();
    return;
  }

  // If not connecting to a network, show the active or connected network.
  const Network* network;
  if (mode_ == DROPDOWN_MODE && cros->connected_network())
    network = cros->connected_network();
  else
    network = cros->active_network();
  if (network) {
    SetActiveNetworkIconAndText(network);
    return;
  }

  // Not connecting, so stop animation.
  animation_connecting_.Stop();

  // No connecting, connected, or active network.
  SetDisconnectedIconAndText();
}

void NetworkMenuIcon::SetActiveNetworkIconAndText(const Network* network) {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  bool animating = false;
  last_network_type_ = network->type();

  // Set icon and badges. Call SetDirty() since network may have changed.
  icon_->SetDirty();
  icon_->UpdateIcon(network);
  // Overlay the VPN badge if connecting to a VPN.
  if (network->type() != TYPE_VPN && cros->virtual_network()) {
    if (cros->virtual_network()->connecting()) {
      const gfx::ImageSkia* vpn_badge = rb.GetImageSkiaNamed(kVpnBadgeId);
      const double animation = GetAnimation();
      animating = true;
      // Even though this is the only place we use vpn_connecting_badge_,
      // it is important that this is a member variable since we set a
      // pointer to it and access that pointer in icon_->GenerateImage().
      vpn_connecting_badge_ = gfx::ImageSkia(
          new FadedImageSource(*vpn_badge, animation), vpn_badge->size());
      icon_->set_bottom_left_badge(&vpn_connecting_badge_);
    }
  }
  if (!animating)
    animation_connecting_.Stop();

  // Set the text to display.
  if (network->type() == TYPE_ETHERNET) {
    if (mode_ == MENU_MODE) {
      text_ = l10n_util::GetStringFUTF16(
          IDS_STATUSBAR_NETWORK_CONNECTED_TOOLTIP,
          l10n_util::GetStringUTF16(
              IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET));
    } else {
      text_ = l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
    }
  } else {
    if (mode_ == MENU_MODE) {
      text_ = l10n_util::GetStringFUTF16(
          IDS_STATUSBAR_NETWORK_CONNECTED_TOOLTIP,
          UTF8ToUTF16(network->name()));
    } else {
      text_ = UTF8ToUTF16(network->name());
    }
  }
}

void NetworkMenuIcon::SetDisconnectedIconAndText() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  switch (last_network_type_) {
    case TYPE_ETHERNET:
      icon_->set_icon(*rb.GetImageSkiaNamed(IDR_STATUSBAR_WIRED));
      break;
    case TYPE_WIFI:
      icon_->set_icon(GetDisconnectedImage(ARCS, resource_color_theme_));
      break;
    case TYPE_WIMAX:
    case TYPE_CELLULAR:
    default:
      icon_->set_icon(GetDisconnectedImage(BARS, resource_color_theme_));
      break;
  }
  icon_->set_bottom_right_badge(
      rb.GetImageSkiaNamed(IDR_STATUSBAR_NETWORK_DISCONNECTED));
  if (mode_ == MENU_MODE)
    text_ = l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_NO_NETWORK_TOOLTIP);
  else
    text_ = l10n_util::GetStringUTF16(IDS_NETWORK_SELECTION_NONE);
}

////////////////////////////////////////////////////////////////////////////////
// Static functions for generating network icon images:

// This defines how we assemble a network icon.
// Currently we iterate over all the available resolutions in |icon|. This will
// be wrong once we dynamically load image resolutions.
// TODO(pkotwicz): Figure out what to do when a new image resolution becomes
// available.
const gfx::ImageSkia NetworkMenuIcon::GenerateImageFromComponents(
    const gfx::ImageSkia& icon,
    const gfx::ImageSkia* top_left_badge,
    const gfx::ImageSkia* top_right_badge,
    const gfx::ImageSkia* bottom_left_badge,
    const gfx::ImageSkia* bottom_right_badge) {
  return gfx::ImageSkia(new NetworkIconImageSource(icon,
                                                   top_left_badge,
                                                   top_right_badge,
                                                   bottom_left_badge,
                                                   bottom_right_badge),
                   icon.size());
}

// We blend connecting icons with a black image to generate a faded icon.
const gfx::ImageSkia NetworkMenuIcon::GenerateConnectingImage(
    const gfx::ImageSkia& source) {
  return gfx::ImageSkia(new FadedImageSource(source, kConnectingImageAlpha),
                        source.size());
}

// Generates and caches an icon image for a network's current state.
const gfx::ImageSkia NetworkMenuIcon::GetImage(const Network* network,
                                               ResourceColorTheme color) {
  DCHECK(network);
  // Maintain a static (global) icon map. Note: Icons are never destroyed;
  // it is assumed that a finite and reasonable number of network icons will be
  // created during a session.

  typedef std::map<std::string, NetworkIcon*> NetworkIconMap;
  static NetworkIconMap* icon_map_dark = NULL;
  static NetworkIconMap* icon_map_light = NULL;
  if (icon_map_dark == NULL)
    icon_map_dark = new NetworkIconMap;
  if (icon_map_light == NULL)
    icon_map_light = new NetworkIconMap;

  NetworkIconMap* icon_map = color == COLOR_DARK ? icon_map_dark :
                                                   icon_map_light;
  // Find or add the icon.
  NetworkIcon* icon;
  NetworkIconMap::iterator iter = icon_map->find(network->service_path());
  if (iter == icon_map->end()) {
    icon = new NetworkIcon(network->service_path(), color);
    icon_map->insert(std::make_pair(network->service_path(), icon));
  } else {
    icon = iter->second;
  }
  // Update and return the icon's image.
  icon->Update();
  return icon->GetImage();
}

// Returns an icon for a disconnected VPN.
const gfx::ImageSkia NetworkMenuIcon::GetVpnImage() {
  static const gfx::ImageSkia *vpn_image = new gfx::ImageSkia(CreateVpnImage());
  return *vpn_image;
}

const gfx::ImageSkia NetworkMenuIcon::GetImage(ImageType type,
                                               int index,
                                               ResourceColorTheme color) {
  NetworkMenuIconSource* source = new NetworkMenuIconSource(type, index, color);
  return gfx::ImageSkia(source, source->size());
}

const gfx::ImageSkia NetworkMenuIcon::GetDisconnectedImage(
    ImageType type,
    ResourceColorTheme color) {
  return GetImage(type, 0, color);
}

const gfx::ImageSkia NetworkMenuIcon::GetConnectedImage(ImageType type,
      ResourceColorTheme color) {
  return GetImage(type, NumImages(type) - 1, color);
}

int NetworkMenuIcon::NumImages(ImageType type) {
  return (type == ARCS) ? kNumArcsImages : kNumBarsImages;
}

}  // chromeos
