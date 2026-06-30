#include "ImuQmi8658.h"
#include <Wire.h>
#include <QMI8658.h>

static QMI8658 imu;

bool ImuQmi8658::begin() {
  // Librería lahavg/QMI8658: begin() retorna bool
  _ok = imu.begin();
  return _ok;
}

bool ImuQmi8658::readAngles(ImuAngles& out) {
  if (!_ok) return false;

  float ax, ay, az;
  float gx, gy, gz;

  if (!imu.readAccel(ax, ay, az)) return false;
  if (!imu.readGyro(gx, gy, gz)) return false;

  // cálculo simple: roll/pitch desde aceleración (estable)
  // roll  = atan2(ay, az)
  // pitch = atan2(-ax, sqrt(ay^2 + az^2))
  out.roll  = atan2f(ay, az) * 180.0f / PI;
  out.pitch = atan2f(-ax, sqrtf(ay*ay + az*az)) * 180.0f / PI;

  return true;
}
