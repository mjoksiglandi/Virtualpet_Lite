#include "ImuQmi8658.h"
#include <Wire.h>
#include <QMI8658.h>

static QMI8658 imu;

bool ImuQmi8658::begin() {
  _ok = imu.begin();
  return _ok;
}

bool ImuQmi8658::readAngles(ImuAngles& out) {
  ImuSample sample{};
  if (!readSample(sample)) return false;
  out = sample.angles;
  return true;
}

bool ImuQmi8658::readSample(ImuSample& out) {
  if (!_ok) return false;

  if (!imu.readAccel(out.ax, out.ay, out.az)) return false;
  if (!imu.readGyro(out.gx, out.gy, out.gz)) return false;

  out.angles.roll = atan2f(out.ay, out.az) * 180.0f / PI;
  out.angles.pitch = atan2f(-out.ax, sqrtf(out.ay * out.ay + out.az * out.az)) * 180.0f / PI;
  return true;
}
