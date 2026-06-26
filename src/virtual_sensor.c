#include <zephyr/devicetree.h>

#define DT_DRV_COMPAT custom_virtual_sensor

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

static int simulated_data = 0;

static int virtual_sensor_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
    if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_AMBIENT_TEMP) {
        return -ENOTSUP;
    }

    simulated_data += 5;
    if (simulated_data > 100) {
        simulated_data = 0;
    }
    
    return 0;
}

static int virtual_sensor_channel_get(const struct device *dev, enum sensor_channel chan, struct sensor_value *val)
{
    if (chan != SENSOR_CHAN_AMBIENT_TEMP) {
        return -ENOTSUP;
    }

    val->val1 = simulated_data;
    val->val2 = 0;
    return 0;
}

static const struct sensor_driver_api virtual_sensor_api_funcs = {
    .sample_fetch = virtual_sensor_sample_fetch,
    .channel_get = virtual_sensor_channel_get,
};

static int virtual_sensor_init(const struct device *dev) {
    printk("[KERNEL] Standard Virtual Sensor Driver Initialized!\n");
    return 0;
}

DEVICE_DT_INST_DEFINE(
    0,                      
    virtual_sensor_init,    
    NULL,                   
    NULL,                   
    NULL,                   
    POST_KERNEL,            
    CONFIG_SENSOR_INIT_PRIORITY,                     
    &virtual_sensor_api_funcs
);

#endif /* DT_HAS_COMPAT_STATUS_OKAY */
