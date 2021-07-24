/*
 * Copyright (c) 2016 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3 as published by
 * the Free Software Foundation.
 *
 */

#include <glib.h>
#include <stdio.h>

#include "accel-mount-matrix.h"

/* The format is the same used in the iio core to export the values:
 * https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/drivers/iio/industrialio-core.c?id=dfc57732ad38f93ae6232a3b4e64fd077383a0f1#n431
 */

static AccelVec3 id_matrix[3] = {
	{ 1.0, 0.0, 0.0 },
	{ 0.0, 1.0, 0.0 },
	{ 0.0, 0.0, 1.0 }
};

static char axis_names[] = "xyz";

AccelVec3 *
setup_mount_matrix (GUdevDevice *device)
{
	AccelVec3 *ret = NULL;
	const char *mount_matrix;

	mount_matrix = g_udev_device_get_property (device, "ACCEL_MOUNT_MATRIX");
	if (mount_matrix) {
		if (parse_mount_matrix (mount_matrix, &ret))
			return ret;

		g_warning ("Failed to parse ACCEL_MOUNT_MATRIX ('%s') from udev",
			   mount_matrix);
		g_clear_pointer (&ret, g_free);
	}

	mount_matrix = g_udev_device_get_sysfs_attr (device, "mount_matrix");
	if (mount_matrix) {
		if (parse_mount_matrix (mount_matrix, &ret))
			return ret;

		g_warning ("Failed to parse mount_matrix ('%s') from sysfs",
			   mount_matrix);
		g_clear_pointer (&ret, g_free);
	}

	/* Some IIO drivers provide multiple sensors via the same sysfs path
	 * and thus they may have different matrices like in a case of
	 * accelerometer and angular velocity for example. The accelerometer
	 * mount matrix is named as in_accel_mount_matrix in that case.
	 *
	 * See https://www.kernel.org/doc/Documentation/ABI/testing/sysfs-bus-iio
	 * for more details. */
	mount_matrix = g_udev_device_get_sysfs_attr (device, "in_accel_mount_matrix");
	if (mount_matrix) {
		if (parse_mount_matrix (mount_matrix, &ret))
			return ret;

		g_warning ("Failed to parse in_accel_mount_matrix ('%s') from sysfs",
			   mount_matrix);
		g_clear_pointer (&ret, g_free);
	}

	/* Linux kernel IIO accelerometer drivers provide mount matrix
	 * via standardized sysfs interface.
	 *
	 * See https://www.kernel.org/doc/Documentation/ABI/testing/sysfs-bus-iio
	 * for more details. */
	mount_matrix = g_udev_device_get_sysfs_attr (device, "in_mount_matrix");
	if (mount_matrix) {
		if (parse_mount_matrix (mount_matrix, &ret))
			return ret;

		g_warning ("Failed to parse in_mount_matrix ('%s') from sysfs",
			   mount_matrix);
		g_clear_pointer (&ret, g_free);
	}

	g_debug ("Failed to auto-detect mount matrix, falling back to identity");
	parse_mount_matrix (NULL, &ret);
	return ret;
}

gboolean
parse_mount_matrix (const char  *mtx,
		    AccelVec3  **vecs)
{
	AccelVec3 *ret;
	guint i;

	g_return_val_if_fail (vecs != NULL, FALSE);


	/* Empty string means we use the identity matrix */
	if (mtx == NULL || *mtx == '\0') {
		*vecs = g_memdup (id_matrix, sizeof(id_matrix));
		return TRUE;
	}

	ret = g_new0 (AccelVec3, 3);
	if (sscanf (mtx, "%f, %f, %f; %f, %f, %f; %f, %f, %f",
		    &ret[0].x, &ret[0].y, &ret[0].z,
		    &ret[1].x, &ret[1].y, &ret[1].z,
		    &ret[2].x, &ret[2].y, &ret[2].z) != 9) {
		g_free (ret);
		g_warning ("Failed to parse '%s' as a mount matrix", mtx);
		return FALSE;
	}

	for (i = 0; i < G_N_ELEMENTS(id_matrix); i++) {
		if (ret[i].x == 0.0f &&
		    ret[i].y == 0.0f &&
		    ret[i].z == 0.0f) {
			g_free (ret);
			g_warning ("In mount matrix '%s', axis %c is all zeroes, which is invalid",
				   mtx, axis_names[i]);
			return FALSE;
		}
	}

	*vecs = ret;

	return TRUE;
}

gboolean
apply_mount_matrix (const AccelVec3  vecs[3],
		    AccelVec3       *accel)
{
	float _x, _y, _z;

	g_return_val_if_fail (accel != NULL, FALSE);

	_x = accel->x * vecs[0].x + accel->y * vecs[0].y + accel->z * vecs[0].z;
	_y = accel->x * vecs[1].x + accel->y * vecs[1].y + accel->z * vecs[1].z;
	_z = accel->x * vecs[2].x + accel->y * vecs[2].y + accel->z * vecs[2].z;

	accel->x = _x;
	accel->y = _y;
	accel->z = _z;

	return TRUE;
}
