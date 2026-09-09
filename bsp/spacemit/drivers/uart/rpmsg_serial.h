/*
 * Copyright (c) 2022-2025, Spacemit
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * RPMSG virtual serial: public definitions shared between the driver and
 * board-level initialisation code.
 */

#ifndef __BSP_RPMSG_SERIAL_H__
#define __BSP_RPMSG_SERIAL_H__

/*
 * RT_RPMSG_SERIAL_NAME - RT-Thread device name for the rpmsg virtual serial.
 *
 * Used in two places:
 *   1. rpmsg_serial.c registers the serial device under this name.
 *   2. board.c redirects the boot console to this device when
 *      BSP_USING_RPMSG_SERIAL is enabled.
 *
 * Changing this name (e.g. to "rpmsg1") is enough to rename the rpmsg serial
 * device without touching either of those files.
 */
#define RT_RPMSG_SERIAL_NAME    "rpmsg0"

#endif /* __BSP_RPMSG_SERIAL_H__ */
