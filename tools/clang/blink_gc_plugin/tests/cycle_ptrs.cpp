// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cycle_ptrs.h"

namespace WebCore {

void A::trace(Visitor* visitor) {
    visitor->trace(m_b);
}

void B::trace(Visitor* visitor) {
    visitor->trace(m_a);
}

}
