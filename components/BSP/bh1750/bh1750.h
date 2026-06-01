#ifndef _BH1750_H
#define _BH1750_H

#include "esp_err.h"

#define bh1750_write_addr 0x46
#define bh1750_read_addr 0x47

#define PowerDown 0x00
#define PowerOn 0x01
#define Reset 0x07
#define HResolutionMode 0x10

esp_err_t bh1750_init(void);
esp_err_t bh1750_read(float *light);

#endif /* _BH1750_H */