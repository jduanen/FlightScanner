#pragma once

#include <cstddef>

namespace services::route_server {

constexpr size_t kMaxUrlLen = 96;

void load();
void saveFromForm(const char* url);
bool hasUrl();
const char* url();

}  // namespace services::route_server
