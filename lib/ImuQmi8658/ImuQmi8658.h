#pragma once
#include <Arduino.h>

struct ImuAngles {
  float pitch = 0.0f;
  float roll  = 0.0f;
};

class ImuQmi8658 {
public:
  bool begin();
  bool readAngles(ImuAngles& out);

private:
  bool _ok = false;
};
