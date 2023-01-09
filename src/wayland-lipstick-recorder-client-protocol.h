/* Generated by wayland-scanner 1.19.0 */

#ifndef LIPSTICK_RECORDER_CLIENT_PROTOCOL_H
#define LIPSTICK_RECORDER_CLIENT_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

#ifdef  __cplusplus
extern "C" {
#endif

/**
 * @page page_lipstick_recorder The lipstick_recorder protocol
 * @section page_ifaces_lipstick_recorder Interfaces
 * - @subpage page_iface_lipstick_recorder_manager - 
 * - @subpage page_iface_lipstick_recorder - 
 * @section page_copyright_lipstick_recorder Copyright
 * <pre>
 *
 * Copyright (C) 2014 Jolla Ltd.
 *
 * Permission to use, copy, modify, distribute, and sell this
 * software and its documentation for any purpose is hereby granted
 * without fee, provided that the above copyright notice appear in
 * all copies and that both that copyright notice and this permission
 * notice appear in supporting documentation, and that the name of
 * the copyright holders not be used in advertising or publicity
 * pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
 * THIS SOFTWARE.
 * </pre>
 */
struct lipstick_recorder;
struct lipstick_recorder_manager;
struct wl_buffer;
struct wl_output;

#ifndef LIPSTICK_RECORDER_MANAGER_INTERFACE
#define LIPSTICK_RECORDER_MANAGER_INTERFACE
/**
 * @page page_iface_lipstick_recorder_manager lipstick_recorder_manager
 * @section page_iface_lipstick_recorder_manager_api API
 * See @ref iface_lipstick_recorder_manager.
 */
/**
 * @defgroup iface_lipstick_recorder_manager The lipstick_recorder_manager interface
 */
extern const struct wl_interface lipstick_recorder_manager_interface;
#endif
#ifndef LIPSTICK_RECORDER_INTERFACE
#define LIPSTICK_RECORDER_INTERFACE
/**
 * @page page_iface_lipstick_recorder lipstick_recorder
 * @section page_iface_lipstick_recorder_api API
 * See @ref iface_lipstick_recorder.
 */
/**
 * @defgroup iface_lipstick_recorder The lipstick_recorder interface
 */
extern const struct wl_interface lipstick_recorder_interface;
#endif

#define LIPSTICK_RECORDER_MANAGER_CREATE_RECORDER 0


/**
 * @ingroup iface_lipstick_recorder_manager
 */
#define LIPSTICK_RECORDER_MANAGER_CREATE_RECORDER_SINCE_VERSION 1

/** @ingroup iface_lipstick_recorder_manager */
static inline void
lipstick_recorder_manager_set_user_data(struct lipstick_recorder_manager *lipstick_recorder_manager, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) lipstick_recorder_manager, user_data);
}

/** @ingroup iface_lipstick_recorder_manager */
static inline void *
lipstick_recorder_manager_get_user_data(struct lipstick_recorder_manager *lipstick_recorder_manager)
{
	return wl_proxy_get_user_data((struct wl_proxy *) lipstick_recorder_manager);
}

static inline uint32_t
lipstick_recorder_manager_get_version(struct lipstick_recorder_manager *lipstick_recorder_manager)
{
	return wl_proxy_get_version((struct wl_proxy *) lipstick_recorder_manager);
}

/** @ingroup iface_lipstick_recorder_manager */
static inline void
lipstick_recorder_manager_destroy(struct lipstick_recorder_manager *lipstick_recorder_manager)
{
	wl_proxy_destroy((struct wl_proxy *) lipstick_recorder_manager);
}

/**
 * @ingroup iface_lipstick_recorder_manager
 *
 * Create a recorder object for the specified output.
 */
static inline struct lipstick_recorder *
lipstick_recorder_manager_create_recorder(struct lipstick_recorder_manager *lipstick_recorder_manager, struct wl_output *output)
{
	struct wl_proxy *recorder;

	recorder = wl_proxy_marshal_constructor((struct wl_proxy *) lipstick_recorder_manager,
			 LIPSTICK_RECORDER_MANAGER_CREATE_RECORDER, &lipstick_recorder_interface, NULL, output);

	return (struct lipstick_recorder *) recorder;
}

#ifndef LIPSTICK_RECORDER_RESULT_ENUM
#define LIPSTICK_RECORDER_RESULT_ENUM
enum lipstick_recorder_result {
	LIPSTICK_RECORDER_RESULT_BAD_BUFFER = 2,
};
#endif /* LIPSTICK_RECORDER_RESULT_ENUM */

#ifndef LIPSTICK_RECORDER_TRANSFORM_ENUM
#define LIPSTICK_RECORDER_TRANSFORM_ENUM
enum lipstick_recorder_transform {
	LIPSTICK_RECORDER_TRANSFORM_NORMAL = 1,
	LIPSTICK_RECORDER_TRANSFORM_Y_INVERTED = 2,
};
#endif /* LIPSTICK_RECORDER_TRANSFORM_ENUM */

/**
 * @ingroup iface_lipstick_recorder
 * @struct lipstick_recorder_listener
 */
struct lipstick_recorder_listener {
	/**
	 * notify the requirements for the frame buffers
	 *
	 * This event will be sent immediately after creation of the
	 * lipstick_recorder object. The wl_buffers the client passes to
	 * the frame request must be big enough to store an image with the
	 * given width, height and format. If they are not the compositor
	 * will send the failed event. If this event is sent again later in
	 * the lifetime of the object the pending frames will be cancelled.
	 *
	 * The format will be one of the values as defined in the
	 * wl_shm::format enum.
	 */
	void (*setup)(void *data,
		      struct lipstick_recorder *lipstick_recorder,
		      int32_t width,
		      int32_t height,
		      int32_t stride,
		      int32_t format);
	/**
	 * notify a frame was recorded, or an error
	 *
	 * The compositor will send this event after a frame was
	 * recorded, or in case an error happened. The client can call
	 * record_frame again to record the next frame.
	 *
	 * 'time' is the time the compositor recorded that frame, in
	 * milliseconds, with an unspecified base.
	 */
	void (*frame)(void *data,
		      struct lipstick_recorder *lipstick_recorder,
		      struct wl_buffer *buffer,
		      uint32_t time,
		      int32_t transform);
	/**
	 * the frame capture failed
	 *
	 * The value of the 'result' argument will be one of the values
	 * of the 'result' enum.
	 */
	void (*failed)(void *data,
		       struct lipstick_recorder *lipstick_recorder,
		       int32_t result,
		       struct wl_buffer *buffer);
	/**
	 * notify a request was cancelled
	 *
	 * The compositor will send this event if the client calls
	 * request_frame more than one time for the same compositor frame.
	 * The cancel event will be sent carrying the old buffer, and the
	 * frame will be recorded using the newest buffer.
	 */
	void (*cancelled)(void *data,
			  struct lipstick_recorder *lipstick_recorder,
			  struct wl_buffer *buffer);
};

/**
 * @ingroup iface_lipstick_recorder
 */
static inline int
lipstick_recorder_add_listener(struct lipstick_recorder *lipstick_recorder,
			       const struct lipstick_recorder_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) lipstick_recorder,
				     (void (**)(void)) listener, data);
}

#define LIPSTICK_RECORDER_DESTROY 0
#define LIPSTICK_RECORDER_RECORD_FRAME 1
#define LIPSTICK_RECORDER_REPAINT 2

/**
 * @ingroup iface_lipstick_recorder
 */
#define LIPSTICK_RECORDER_SETUP_SINCE_VERSION 1
/**
 * @ingroup iface_lipstick_recorder
 */
#define LIPSTICK_RECORDER_FRAME_SINCE_VERSION 1
/**
 * @ingroup iface_lipstick_recorder
 */
#define LIPSTICK_RECORDER_FAILED_SINCE_VERSION 1
/**
 * @ingroup iface_lipstick_recorder
 */
#define LIPSTICK_RECORDER_CANCELLED_SINCE_VERSION 1

/**
 * @ingroup iface_lipstick_recorder
 */
#define LIPSTICK_RECORDER_DESTROY_SINCE_VERSION 1
/**
 * @ingroup iface_lipstick_recorder
 */
#define LIPSTICK_RECORDER_RECORD_FRAME_SINCE_VERSION 1
/**
 * @ingroup iface_lipstick_recorder
 */
#define LIPSTICK_RECORDER_REPAINT_SINCE_VERSION 1

/** @ingroup iface_lipstick_recorder */
static inline void
lipstick_recorder_set_user_data(struct lipstick_recorder *lipstick_recorder, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) lipstick_recorder, user_data);
}

/** @ingroup iface_lipstick_recorder */
static inline void *
lipstick_recorder_get_user_data(struct lipstick_recorder *lipstick_recorder)
{
	return wl_proxy_get_user_data((struct wl_proxy *) lipstick_recorder);
}

static inline uint32_t
lipstick_recorder_get_version(struct lipstick_recorder *lipstick_recorder)
{
	return wl_proxy_get_version((struct wl_proxy *) lipstick_recorder);
}

/**
 * @ingroup iface_lipstick_recorder
 *
 * Destroy the recorder object, discarding any frame request
 * that may be pending.
 */
static inline void
lipstick_recorder_destroy(struct lipstick_recorder *lipstick_recorder)
{
	wl_proxy_marshal((struct wl_proxy *) lipstick_recorder,
			 LIPSTICK_RECORDER_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) lipstick_recorder);
}

/**
 * @ingroup iface_lipstick_recorder
 *
 * Ask the compositor to record its next frame, putting
 * the content into the specified buffer data. The frame
 * event will be sent when the frame is recorded.
 * Only one frame will be recorded, the client will have
 * to call this again after the frame event if it wants to
 * record more frames.
 *
 * The buffer must be a shm buffer, trying to use another
 * type of buffer will result in failure to capture the
 * frame and the failed event will be sent.
 */
static inline void
lipstick_recorder_record_frame(struct lipstick_recorder *lipstick_recorder, struct wl_buffer *buffer)
{
	wl_proxy_marshal((struct wl_proxy *) lipstick_recorder,
			 LIPSTICK_RECORDER_RECORD_FRAME, buffer);
}

/**
 * @ingroup iface_lipstick_recorder
 *
 * Calling record_frame will not cause the compositor to
 * repaint, but it will wait instead for the first frame
 * the compositor draws due to some other external event
 * or internal change.
 * Calling this request after calling record_frame will
 * ask the compositor to redraw as soon at possible even
 * if it wouldn't otherwise.
 * If no frame was requested this request has no effect.
 */
static inline void
lipstick_recorder_repaint(struct lipstick_recorder *lipstick_recorder)
{
	wl_proxy_marshal((struct wl_proxy *) lipstick_recorder,
			 LIPSTICK_RECORDER_REPAINT);
}

#ifdef  __cplusplus
}
#endif

#endif