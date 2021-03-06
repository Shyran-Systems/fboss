/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "fboss/agent/hw/test/ConfigFactory.h"
#include "fboss/agent/hw/test/HwLinkStateDependentTest.h"
#include "fboss/agent/hw/test/HwTestPacketUtils.h"
#include "fboss/agent/hw/test/TrafficPolicyUtils.h"
#include "fboss/agent/test/EcmpSetupHelper.h"

#include <folly/IPAddress.h>

namespace facebook::fboss {

class HwWatermarkTest : public HwLinkStateDependentTest {
 private:
  cfg::SwitchConfig initialConfig() const override {
    auto cfg = utility::oneL3IntfConfig(
        getHwSwitch(), masterLogicalPortIds()[0], cfg::PortLoopbackMode::MAC);
    return cfg;
  }

  MacAddress kDstMac() const {
    return getPlatform()->getLocalMac();
  }

  template <typename ECMP_HELPER>
  std::shared_ptr<SwitchState> setupECMPForwarding(
      const ECMP_HELPER& ecmpHelper) {
    auto kEcmpWidthForTest = 1;
    auto newState = ecmpHelper.setupECMPForwarding(
        ecmpHelper.resolveNextHops(getProgrammedState(), kEcmpWidthForTest),
        kEcmpWidthForTest);
    applyNewState(newState);
    return getProgrammedState();
  }

  void sendUdpPkt(uint8_t dscpVal) {
    auto vlanId = utility::firstVlanID(initialConfig());
    auto intfMac = utility::getInterfaceMac(getProgrammedState(), vlanId);

    auto txPacket = utility::makeUDPTxPacket(
        getHwSwitch(),
        vlanId,
        intfMac,
        intfMac,
        folly::IPAddressV6("2620:0:1cfe:face:b00c::3"),
        folly::IPAddressV6("2620:0:1cfe:face:b00c::4"),
        8000,
        8001,
        // Trailing 2 bits are for ECN
        static_cast<uint8_t>(dscpVal << 2),
        255,
        std::vector<uint8_t>(6000, 0xff));
    getHwSwitch()->sendPacketSwitchedSync(std::move(txPacket));
  }

 protected:
  /*
   * In practice, sending single packet usually (but not always) returned BST
   * value > 0 (usually 2, but  other times it was 0). Thus, send a large
   * number of packets to try to avoid the risk of flakiness.
   */
  void sendUdpPkts(uint8_t dscpVal, int cnt = 100) {
    for (int i = 0; i < cnt; i++) {
      sendUdpPkt(dscpVal);
    }
  }
  void _createAcl(uint8_t dscpVal, int queueId, const std::string& aclName) {
    auto newCfg{initialConfig()};
    utility::addDscpAclToCfg(&newCfg, aclName, dscpVal);
    utility::addQueueMatcher(&newCfg, aclName, queueId);
    applyNewConfig(newCfg);
  }

  void _setup(uint8_t kDscp) {
    utility::EcmpSetupAnyNPorts6 ecmpHelper6{getProgrammedState()};
    setupECMPForwarding(ecmpHelper6);
  }

  void
  assertWatermark(PortID port, int queueId, bool expectZero, int retries = 1) {
    EXPECT_TRUE(gotExpectedWatermark(port, queueId, expectZero, retries));
  }

 private:
  bool
  gotExpectedWatermark(PortID port, int queueId, bool expectZero, int retries) {
    do {
      auto queueWaterMarks = *getHwSwitchEnsemble()
                                  ->getLatestPortStats({port})[port]
                                  .queueWatermarkBytes__ref();
      XLOG(DBG0) << "queueId: " << queueId
                 << " Watermark: " << queueWaterMarks[queueId];
      auto watermarkAsExpected = (expectZero && !queueWaterMarks[queueId]) ||
          (!expectZero && queueWaterMarks[queueId]);
      if (!retries || watermarkAsExpected) {
        return true;
      }
      XLOG(DBG0) << " Retry ...";
      sleep(1);
    } while (--retries > 0);
    XLOG(INFO) << " Did not get expected watermark value";
    return false;
  }
};

TEST_F(HwWatermarkTest, VerifyDefaultQueue) {
  uint8_t kDscp = 10;
  int kDefQueueId = 0;

  auto setup = [=]() { _setup(kDscp); };
  auto verify = [=]() {
    sendUdpPkts(kDscp);
    // Assert non zero watermark
    assertWatermark(masterLogicalPortIds()[0], kDefQueueId, false);
    // Assert zero watermark
    assertWatermark(masterLogicalPortIds()[0], kDefQueueId, true, 5);
  };

  verifyAcrossWarmBoots(setup, verify);
}

TEST_F(HwWatermarkTest, VerifyNonDefaultQueue) {
  if (!isSupported(HwAsic::Feature::L3_QOS)) {
    return;
  }

  uint8_t kDscp = 10;
  int kNonDefQueueId = 1;
  auto kAclName = "acl1";

  auto setup = [=]() {
    _createAcl(kDscp, kNonDefQueueId, kAclName);
    _setup(kDscp);
  };
  auto verify = [=]() {
    sendUdpPkts(kDscp);
    // Assert non zero watermark
    assertWatermark(masterLogicalPortIds()[0], kNonDefQueueId, false);
    // Assert zero watermark
    assertWatermark(masterLogicalPortIds()[0], kNonDefQueueId, true, 5);
  };

  verifyAcrossWarmBoots(setup, verify);
}

} // namespace facebook::fboss
