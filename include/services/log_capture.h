#pragma once

#include <Print.h>
#include <cstddef>

/** Tees all output to hardware Serial and an in-RAM ring buffer (cleared on reboot). */
class LogCapture : public Print {
public:
  size_t write(uint8_t b) override;
  size_t write(const uint8_t* buf, size_t size) override;

  const char* buffer() const;
  size_t used() const;
  bool full() const;
};

extern LogCapture Log;
