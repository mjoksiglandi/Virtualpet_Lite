#pragma once
#include <Arduino.h>

struct ImuAngles {
  float pitch = 0.0f;
  float roll  = 0.0f;
};

struct ImuSample {
  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  float gx = 0.0f;
  float gy = 0.0f;
  float gz = 0.0f;
  ImuAngles angles;
};

class ImuQmi8658 {
public:
  bool begin();
  bool readAngles(ImuAngles& out);
  bool readSample(ImuSample& out);

private:
  bool _ok = false;
};
