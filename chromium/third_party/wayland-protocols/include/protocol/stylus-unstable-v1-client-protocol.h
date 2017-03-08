/* Generated by wayland-scanner 1.11.0 */

#ifndef STYLUS_UNSTABLE_V1_CLIENT_PROTOCOL_H
#define STYLUS_UNSTABLE_V1_CLIENT_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

#ifdef  __cplusplus
extern "C" {
#endif

/**
 * @page page_stylus_unstable_v1 The stylus_unstable_v1 protocol
 * @section page_ifaces_stylus_unstable_v1 Interfaces
 * - @subpage page_iface_zcr_stylus_v1 - extends wl_pointer with events for on-screen stylus
 * - @subpage page_iface_zcr_pointer_stylus_v1 - stylus extension for pointer
 * @section page_copyright_stylus_unstable_v1 Copyright
 * <pre>
 *
 * Copyright 2016 The Chromium Authors.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 * </pre>
 */
struct wl_pointer;
struct zcr_pointer_stylus_v1;
struct zcr_stylus_v1;

/**
 * @page page_iface_zcr_stylus_v1 zcr_stylus_v1
 * @section page_iface_zcr_stylus_v1_desc Description
 *
 * Allows a wl_pointer to represent an on-screen stylus. The client can
 * interpret the on-screen stylus like any other mouse device, and use
 * this protocol to obtain detail information about the type of stylus,
 * as well as the force and tilt of the tool.
 *
 * These events are to be fired by the server within the same frame as other
 * wl_pointer events.
 *
 * Warning! The protocol described in this file is experimental and
 * backward incompatible changes may be made. Backward compatible changes
 * may be added together with the corresponding uinterface version bump.
 * Backward incompatible changes are done by bumping the version number in
 * the protocol and uinterface names and resetting the interface version.
 * Once the protocol is to be declared stable, the 'z' prefix and the
 * version number in the protocol and interface names are removed and the
 * interface version number is reset.
 * @section page_iface_zcr_stylus_v1_api API
 * See @ref iface_zcr_stylus_v1.
 */
/**
 * @defgroup iface_zcr_stylus_v1 The zcr_stylus_v1 interface
 *
 * Allows a wl_pointer to represent an on-screen stylus. The client can
 * interpret the on-screen stylus like any other mouse device, and use
 * this protocol to obtain detail information about the type of stylus,
 * as well as the force and tilt of the tool.
 *
 * These events are to be fired by the server within the same frame as other
 * wl_pointer events.
 *
 * Warning! The protocol described in this file is experimental and
 * backward incompatible changes may be made. Backward compatible changes
 * may be added together with the corresponding uinterface version bump.
 * Backward incompatible changes are done by bumping the version number in
 * the protocol and uinterface names and resetting the interface version.
 * Once the protocol is to be declared stable, the 'z' prefix and the
 * version number in the protocol and interface names are removed and the
 * interface version number is reset.
 */
extern const struct wl_interface zcr_stylus_v1_interface;
/**
 * @page page_iface_zcr_pointer_stylus_v1 zcr_pointer_stylus_v1
 * @section page_iface_zcr_pointer_stylus_v1_desc Description
 *
 * The zcr_pointer_stylus_v1 interface extends the wl_pointer interface with
 * events to describe details about a stylus acting as a pointer.
 * @section page_iface_zcr_pointer_stylus_v1_api API
 * See @ref iface_zcr_pointer_stylus_v1.
 */
/**
 * @defgroup iface_zcr_pointer_stylus_v1 The zcr_pointer_stylus_v1 interface
 *
 * The zcr_pointer_stylus_v1 interface extends the wl_pointer interface with
 * events to describe details about a stylus acting as a pointer.
 */
extern const struct wl_interface zcr_pointer_stylus_v1_interface;

#ifndef ZCR_STYLUS_V1_ERROR_ENUM
#define ZCR_STYLUS_V1_ERROR_ENUM
enum zcr_stylus_v1_error {
	/**
	 * the pointer already has a pointer_stylus object associated
	 */
	ZCR_STYLUS_V1_ERROR_POINTER_STYLUS_EXISTS = 0,
};
#endif /* ZCR_STYLUS_V1_ERROR_ENUM */

#define ZCR_STYLUS_V1_GET_POINTER_STYLUS	0

/**
 * @ingroup iface_zcr_stylus_v1
 */
#define ZCR_STYLUS_V1_GET_POINTER_STYLUS_SINCE_VERSION	1

/** @ingroup iface_zcr_stylus_v1 */
static inline void
zcr_stylus_v1_set_user_data(struct zcr_stylus_v1 *zcr_stylus_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zcr_stylus_v1, user_data);
}

/** @ingroup iface_zcr_stylus_v1 */
static inline void *
zcr_stylus_v1_get_user_data(struct zcr_stylus_v1 *zcr_stylus_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zcr_stylus_v1);
}

static inline uint32_t
zcr_stylus_v1_get_version(struct zcr_stylus_v1 *zcr_stylus_v1)
{
	return wl_proxy_get_version((struct wl_proxy *) zcr_stylus_v1);
}

/** @ingroup iface_zcr_stylus_v1 */
static inline void
zcr_stylus_v1_destroy(struct zcr_stylus_v1 *zcr_stylus_v1)
{
	wl_proxy_destroy((struct wl_proxy *) zcr_stylus_v1);
}

/**
 * @ingroup iface_zcr_stylus_v1
 *
 * Create pointer_stylus object. See zcr_pointer_stylus_v1 interface for
 * details. If the given wl_pointer already has a pointer_stylus object
 * associated, the pointer_stylus_exists protocol error is raised.
 */
static inline struct zcr_pointer_stylus_v1 *
zcr_stylus_v1_get_pointer_stylus(struct zcr_stylus_v1 *zcr_stylus_v1, struct wl_pointer *pointer)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) zcr_stylus_v1,
			 ZCR_STYLUS_V1_GET_POINTER_STYLUS, &zcr_pointer_stylus_v1_interface, NULL, pointer);

	return (struct zcr_pointer_stylus_v1 *) id;
}

#ifndef ZCR_POINTER_STYLUS_V1_TOOL_TYPE_ENUM
#define ZCR_POINTER_STYLUS_V1_TOOL_TYPE_ENUM
/**
 * @ingroup iface_zcr_pointer_stylus_v1
 * tool type of device.
 */
enum zcr_pointer_stylus_v1_tool_type {
	/**
	 * Mouse or touchpad, not a stylus.
	 */
	ZCR_POINTER_STYLUS_V1_TOOL_TYPE_MOUSE = 0,
	/**
	 * Pen
	 */
	ZCR_POINTER_STYLUS_V1_TOOL_TYPE_PEN = 1,
	/**
	 * Touch
	 */
	ZCR_POINTER_STYLUS_V1_TOOL_TYPE_TOUCH = 2,
	/**
	 * Eraser
	 */
	ZCR_POINTER_STYLUS_V1_TOOL_TYPE_ERASER = 3,
};
#endif /* ZCR_POINTER_STYLUS_V1_TOOL_TYPE_ENUM */

/**
 * @ingroup iface_zcr_pointer_stylus_v1
 * @struct zcr_pointer_stylus_v1_listener
 */
struct zcr_pointer_stylus_v1_listener {
	/**
	 * pointing device tool type changed
	 *
	 * Notification that the user is using a new tool type. There can
	 * only be one tool in use at a time. If the pointer enters a
	 * client surface, with a tool type other than mouse, this event
	 * will also be generated.
	 *
	 * If this event is not received, the client has to assume a mouse
	 * is in use. The remaining events of this protocol are only being
	 * generated after this event has been fired with a tool type other
	 * than mouse.
	 * @param type new device type
	 */
	void (*tool_change)(void *data,
			    struct zcr_pointer_stylus_v1 *zcr_pointer_stylus_v1,
			    uint32_t type);
	/**
	 * force change event
	 *
	 * Notification of a change in physical force on the surface of
	 * the screen.
	 *
	 * If the pointer enters a client surface, with a tool type other
	 * than mouse, this event will also be generated.
	 *
	 * The force is calibrated and normalized to the 0 to 1 range.
	 * @param time timestamp with millisecond granularity
	 * @param force new value of force
	 */
	void (*force)(void *data,
		      struct zcr_pointer_stylus_v1 *zcr_pointer_stylus_v1,
		      uint32_t time,
		      wl_fixed_t force);
	/**
	 * force change event
	 *
	 * Notification of a change in tilt of the pointing tool.
	 *
	 * If the pointer enters a client surface, with a tool type other
	 * than mouse, this event will also be generated.
	 *
	 * Measured from surface normal as plane angle in degrees, values
	 * lie in [-90,90]. A positive x is to the right and a positive y
	 * is towards the user.
	 * @param time timestamp with millisecond granularity
	 * @param tilt_x tilt in x direction
	 * @param tilt_y tilt in y direction
	 */
	void (*tilt)(void *data,
		     struct zcr_pointer_stylus_v1 *zcr_pointer_stylus_v1,
		     uint32_t time,
		     wl_fixed_t tilt_x,
		     wl_fixed_t tilt_y);
};

/**
 * @ingroup zcr_pointer_stylus_v1_iface
 */
static inline int
zcr_pointer_stylus_v1_add_listener(struct zcr_pointer_stylus_v1 *zcr_pointer_stylus_v1,
				   const struct zcr_pointer_stylus_v1_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) zcr_pointer_stylus_v1,
				     (void (**)(void)) listener, data);
}

#define ZCR_POINTER_STYLUS_V1_DESTROY	0

/**
 * @ingroup iface_zcr_pointer_stylus_v1
 */
#define ZCR_POINTER_STYLUS_V1_DESTROY_SINCE_VERSION	1

/** @ingroup iface_zcr_pointer_stylus_v1 */
static inline void
zcr_pointer_stylus_v1_set_user_data(struct zcr_pointer_stylus_v1 *zcr_pointer_stylus_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zcr_pointer_stylus_v1, user_data);
}

/** @ingroup iface_zcr_pointer_stylus_v1 */
static inline void *
zcr_pointer_stylus_v1_get_user_data(struct zcr_pointer_stylus_v1 *zcr_pointer_stylus_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zcr_pointer_stylus_v1);
}

static inline uint32_t
zcr_pointer_stylus_v1_get_version(struct zcr_pointer_stylus_v1 *zcr_pointer_stylus_v1)
{
	return wl_proxy_get_version((struct wl_proxy *) zcr_pointer_stylus_v1);
}

/**
 * @ingroup iface_zcr_pointer_stylus_v1
 */
static inline void
zcr_pointer_stylus_v1_destroy(struct zcr_pointer_stylus_v1 *zcr_pointer_stylus_v1)
{
	wl_proxy_marshal((struct wl_proxy *) zcr_pointer_stylus_v1,
			 ZCR_POINTER_STYLUS_V1_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) zcr_pointer_stylus_v1);
}

#ifdef  __cplusplus
}
#endif

#endif
