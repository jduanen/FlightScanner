#include "services/log_capture.h"

#include <Arduino.h>
#include <cstring>

namespace {

constexpr size_t kCapacity = 16 * 1024;
char s_buf[kCapacity];
size_t s_used = 0;

}  // namespace

size_t LogCapture::write(uint8_t b) {
  Serial.write(b);
  if (s_used < kCapacity - 1) {
    s_buf[s_used++] = static_cast<char>(b);
    s_buf[s_used] = '\0';
  }
  return 1;
}

size_t LogCapture::write(const uint8_t* buf, size_t size) {
  Serial.write(buf, size);
  if (s_used < kCapacity - 1) {
    const size_t avail = kCapacity - 1 - s_used;
    const size_t n = size < avail ? size : avail;
    memcpy(s_buf + s_used, buf, n);
    s_used += n;
    s_buf[s_used] = '\0';
  }
  return size;
}

const char* LogCapture::buffer() const { return s_buf; }
size_t LogCapture::used() const { return s_used; }
bool LogCapture::full() const { return s_used >= kCapacity - 1; }

LogCapture Log;
