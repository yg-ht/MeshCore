#pragma once

#include <bluefruit.h>

#if defined(NRF52_PLATFORM)

// Cancels the application OTA idle guard once the DFU handoff has begun.
void meshcore_nrf52_cancel_ota_timeout();

class MeshCoreNrf52Dfu : public BLEService {
protected:
  BLECharacteristic _chr_control;

public:
  MeshCoreNrf52Dfu();

  err_t begin() override;
};

#endif
