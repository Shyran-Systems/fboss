/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#include "BcmHostKey.h"

#include "fboss/agent/FbossError.h"

namespace facebook { namespace fboss {

BcmHostKey::BcmHostKey(
    opennsl_vrf_t vrf,
    folly::IPAddress ipAddr,
    folly::Optional<InterfaceID> intf)
    : vrf_(vrf), addr_(std::move(ipAddr)), intfID_(intf) {
  // need the interface ID if and only if the address is v6 link-local
  if (addr().isV6() && addr().isLinkLocal()) {
    if (!intfID().hasValue()) {
      throw FbossError(
          "Missing interface scoping for link-local address {}.", addr().str());
    }
  } else {
    // for not v6 link-local address, do not track the interface ID
    intfID_ = folly::none;
  }
}

std::string BcmHostKey::str() const {
  std::string intfStr = "";
  if (intfID_) {
    intfStr = folly::to<std::string>("@I", intfID_.value());
  }
  return folly::to<std::string>("BcmHost: ", addr_, intfStr, "@vrf", getVrf());
}

bool operator==(const BcmHostKey& a, const BcmHostKey& b) {
  return (
      a.getVrf() == b.getVrf() && a.addr() == b.addr() &&
      a.intfID() == b.intfID());
}

bool operator< (const BcmHostKey& a, const BcmHostKey& b) {
  if (a.getVrf() != b.getVrf()) {
    return a.getVrf() < b.getVrf();
  } else if (a.intfID() != b.intfID()) {
    return a.intfID() < b.intfID();
  } else {
    return a.addr() < b.addr();
  }
}

void toAppend(const HostKey& key, std::string* result) {
  result->append(key.str());
}

std::ostream& operator<<(std::ostream& os, const HostKey& key) {
  return os << key.str();
}

std::string BcmLabeledHostKey::str() const {
  return folly::to<std::string>(
      "BcmHost: ", addr_, "@I", intfID_, "@label", getLabel());
}

bool operator==(const BcmLabeledHostKey& lhs, const BcmLabeledHostKey& rhs) {
  return lhs.getVrf() == rhs.getVrf() && lhs.getLabel() == rhs.getLabel() &&
      lhs.addr() == rhs.addr() && lhs.intfID() == rhs.intfID() &&
      lhs.labels_ == rhs.labels_;
}

bool operator<(const BcmLabeledHostKey& lhs, const BcmLabeledHostKey& rhs) {
  return std::tie(lhs.vrf_, lhs.label_, lhs.intfID_, lhs.addr_, lhs.labels_) <
      std::tie(rhs.vrf_, rhs.label_, rhs.intfID_, rhs.addr_, rhs.labels_);
}

template <class SelfType>
bool operator<(SelfType const& lhs, SelfType const& rhs) {
  return folly::poly_call<6>(lhs, rhs);
}

template <class SelfType>
bool operator==(SelfType const& lhs, SelfType const& rhs) {
  return folly::poly_call<7>(lhs, rhs);
}

HostKey getNextHopKey(opennsl_vrf_t vrf, const NextHop& nexthop) {
  if (!nexthop.labelForwardingAction() || !nexthop.isResolved() ||
      (nexthop.labelForwardingAction()->type() !=
          LabelForwardingAction::LabelForwardingType::SWAP &&
      nexthop.labelForwardingAction()->type() !=
          LabelForwardingAction::LabelForwardingType::PUSH)) {
    // unlabaled next hop
    return BcmHostKey(vrf, nexthop);
  }
  if (nexthop.labelForwardingAction()->type() ==
      LabelForwardingAction::LabelForwardingType::SWAP) {
    // labeled next hop for swap
    return BcmLabeledHostKey(
        vrf,
        nexthop.labelForwardingAction()->swapWith().value(),
        nexthop.addr(),
        nexthop.intfID().value());
  }
  // labeled next hop for push
  return BcmLabeledHostKey(
      vrf,
      nexthop.labelForwardingAction()->pushStack().value(),
      nexthop.addr(),
      nexthop.intfID().value());
}
}}
