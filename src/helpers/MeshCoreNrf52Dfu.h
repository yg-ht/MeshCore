#pragma once

#include <bluefruit.h>

#if defined(NRF52_PLATFORM)

class MeshCoreNrf52Dfu : public BLEService {
protected:
  BLECharacteristic _chr_control;

public:
  MeshCoreNrf52Dfu();

  err_t begin() override;
};

#endif
