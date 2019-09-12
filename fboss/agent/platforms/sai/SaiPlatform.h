/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#pragma once

#include "fboss/agent/AgentConfig.h"
#include "fboss/agent/HwSwitch.h"
#include "fboss/agent/Platform.h"
#include "fboss/agent/ThriftHandler.h"
#include "fboss/agent/hw/sai/switch/SaiSwitch.h"
#include "fboss/agent/platforms/common/PlatformProductInfo.h"
#include "fboss/agent/platforms/sai/SaiPlatformPort.h"

#include <memory>
#include <vector>

extern "C" {
#include <sai.h>
#include <saistatus.h>
#include <saiswitch.h>
}

namespace facebook {
namespace fboss {

class SaiPlatform : public Platform {
 public:
  explicit SaiPlatform(std::unique_ptr<PlatformProductInfo> productInfo);
  HwSwitch* getHwSwitch() const override;
  void onHwInitialized(SwSwitch* sw) override;
  void onInitialConfigApplied(SwSwitch* sw) override;
  std::unique_ptr<ThriftHandler> createHandler(SwSwitch* sw) override;
  folly::MacAddress getLocalMac() const override;
  void getProductInfo(ProductInfo& info) override;
  TransceiverIdxThrift getPortMapping(PortID port) const override;
  virtual folly::Optional<std::string> getPlatformAttribute(
      cfg::PlatformAttributes platformAttribute);
  virtual SaiPlatformPort* getPort(PortID id) const;
  virtual PlatformMode getMode() const;
  PlatformPort* getPlatformPort(PortID port) const override;
  virtual void initPorts();
  virtual std::string getHwConfig() = 0;
  std::string getHwConfigDumpFile();
  void generateHwConfigFile();
  sai_service_method_table_t* getServiceMethodTable() const;
  void stop() override;

  /*
   * Get ids of all controlling ports
   */
  virtual std::vector<PortID> masterLogicalPortIds() const {
    // TODO make this pure virtual when we cook up a platform
    // for fake SAI
    return {};
  }

 private:
  void initImpl() override;
  void initSaiProfileValues();
  std::unique_ptr<SaiSwitch> saiSwitch_;
  const std::unique_ptr<PlatformProductInfo> productInfo_;
  std::unordered_map<PortID, std::unique_ptr<SaiPlatformPort>> portMapping_;
};

} // namespace fboss
} // namespace facebook
