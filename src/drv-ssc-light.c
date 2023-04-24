/*
 * Copyright (c) 2023-2025 Dylan Van Assche
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3 as published by
 * the Free Software Foundation.
 */

#include "drivers.h"

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <gio/gio.h>
#include <libssc-sensor.h>
#include <libssc-sensor-light.h>

typedef struct DrvData {
	SSCSensorLight *sensor;
	guint measurement_id;
} DrvData;

static gboolean
ssc_light_discover (GUdevDevice *device)
{
	SSCSensorLight *sensor;

	/* Verify presence of FastRPC device */
	if (!drv_check_udev_sensor_type (device, "ssc-light", NULL))
		return FALSE;

	/* Open and close SSC light sensor for discovering */
	sensor = ssc_sensor_light_new_sync (NULL, NULL);
	if (!sensor)
		return FALSE;

	if (!ssc_sensor_light_close_sync (sensor, NULL, NULL)) {
		g_clear_object (&sensor);
		return FALSE;
	}

	g_clear_object (&sensor);

	g_debug ("Found SSC light at %s", g_udev_device_get_sysfs_path (device));
	return TRUE;
}

static void
measurement_cb (SSCSensorLight *sensor, gfloat intensity, gpointer user_data)
{
	SensorDevice *sensor_device = user_data;
	LightReadings readings;

	readings.level = intensity;
	readings.uses_lux = TRUE;
	sensor_device->callback_func (sensor_device, (gpointer) &readings, sensor_device->user_data);
}

static SensorDevice *
ssc_light_open (GUdevDevice *device)
{
	SensorDevice *sensor_device;
	DrvData *drv_data;
	g_autoptr (GError) error = NULL;

	sensor_device = g_new0 (SensorDevice, 1);
	sensor_device->priv = g_new0 (DrvData, 1);
	drv_data = (DrvData *) sensor_device->priv;

	/* Create sensor */
	drv_data->sensor = ssc_sensor_light_new_sync (NULL, &error);
	if (!drv_data->sensor) {
		g_warning ("Creating SSC light sensor failed: %s", error->message);
		g_clear_pointer (&sensor_device->priv, g_free);
		g_free (sensor_device);
		return NULL;
	}
	g_object_get (drv_data->sensor,
                      SSC_SENSOR_NAME, &sensor_device->name,
		      NULL);

	/* Start listening for measurements */
	drv_data->measurement_id = g_signal_connect (drv_data->sensor,
			                             "measurement",
						     G_CALLBACK (measurement_cb),
						     sensor_device);

	/* Enable sensor */
	if (!ssc_sensor_light_open_sync (drv_data->sensor, NULL, &error)) {
		g_warning ("Opening SSC light sensor failed: %s", error->message);
		g_clear_object (&drv_data->sensor);
		g_clear_pointer (&sensor_device->priv, g_free);
		g_free (sensor_device);
		return NULL;
	}

	return sensor_device;
}

static void
ssc_light_close (SensorDevice *sensor_device)
{
	g_autoptr (GError) error = NULL;
	DrvData *drv_data = (DrvData *) sensor_device->priv;

	/* Stop listening for measurements */
	g_warn_if_fail (drv_data->measurement_id > 0);
	g_signal_handler_disconnect (drv_data->sensor, drv_data->measurement_id);

	/* Disable sensor */
	if (!ssc_sensor_light_close_sync (drv_data->sensor, NULL, &error))
		g_warning ("Closing SSC light sensor failed: %s", error->message);

	g_clear_object (&drv_data->sensor);
	g_clear_pointer (&sensor_device->priv, g_free);
	g_free (sensor_device);
}

SensorDriver ssc_light = {
	.driver_name = "SSC light sensor",
	.type = DRIVER_TYPE_LIGHT,

	.discover = ssc_light_discover,
	.open = ssc_light_open,
	.close = ssc_light_close,
};
