#ifndef __DHT11_H__
#define __DHT11_H__

#include <driver/gpio.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        float humidity;
        float temperature;
    } dht11_data_t;

    esp_err_t dht11_init(gpio_num_t pin);

    esp_err_t dht11_read(dht11_data_t *data);

#ifdef __cplusplus
}
#endif

#endif