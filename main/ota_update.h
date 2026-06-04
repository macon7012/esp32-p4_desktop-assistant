#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ota_update_start(const char *url);
void ota_trigger(const char *url);

#ifdef __cplusplus
}
#endif