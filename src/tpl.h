#ifndef TPL_H
#define TPL_H

/**
 * @file tpl.h
 * @brief TPL API header file.
 *
 * TPL is an abstraction layer for surface & buffer management on Tizen
 * platform aimed to implement the EGL porting layer of ARM Mali OpenGLES
 * driver over various display protocols.
 *
 * TPL provides object-oriented interfaces. Every TPL object can be represented
 * as generic tpl_object_t which is referenced-counted and provides common
 * functions. Currently, following types of objects are provided.
 *
 * Display
 * Surface
 * Buffer
 *
 * Display, like a normal display, represents a display system which is usually
 * used for connection to the server, scope for other objects.
 *
 * Surface correponds to a native surface like X drawable or wl_surface.
 * A surface might be configured to use N-buffers. (usually double-buffered or
 * tripple-buffered).
 *
 * Buffer is actually something we can render on it, usually a set of pixels or
 * block of memory.
 *
 * Here is a simple example
 *
 * dpy = tpl_display_get(NULL);
 * sfc = tpl_surface_create(dpy, ...);
 *
 * while (1)
 * {
 *	tpl_surface_begin_frame(sfc);
 *	buf = tpl_surface_get_buffer(sfc);
 *
 *	draw something...
 *
 *	tpl_surface_end_frame(sfc);
 *	tpl_surface_post(sfc);
 * }
 *
 * In Mali driver, "draw something..." part is what Mali frame builder does.
 *
 * TPL exposes native platform buffer identifiers and managers so that the
 * buffer can be used in other modules. Currently, dma_buf/DRM is supported for
 * such kind of purposes.
 *
 * EGL porting layer just calls TPL functions to do what it is requested, and
 * give the result to Mali driver. TPL does all the protocol dependent actions.
 * Such protocol dependent part can be well-separated into TPL backends.
 *
 * Also, TPL backend can be configured at runtime. Users can specify which type
 * of backend to use when initializing a display object.
 *
 * For detailed API semantics, please refer to the API documentations.
 */

#define TPL_TRUE	1
#define TPL_FALSE	0

#define TPL_DONT_CARE -1

/**
 * Boolean variable type.
 *
 * TPL_TRUE or TPL_FALSE
 */
typedef unsigned int tpl_bool_t;

/**
 * Handle to native objects.
 *
 * Represent a handle to a native object like pixmap, window, wl_display and
 * etc.
 */
typedef void * tpl_handle_t;

/**
 * Structure containing function pointers to DDK's EGL layer.
 *
 * TPL needs to call DDK specific functions such as atomic operations and hash
 * tables. The necessary function pointers are registered in this structure.
 */
#include <stdlib.h>

typedef volatile unsigned int tpl_util_atomic_uint;

/**
 * A structure representing generic TPL object.
 *
 * Generic base class type for various TPL objects.
 */
typedef struct _tpl_object tpl_object_t;

/**
 * A structure representing TPL display object.
 *
 * TPL display is an object representing a system which is used to display
 * things. This is similar in concept with native displays such as X Display
 * and wl_display. TPL display is used for following items.
 *
 * 1. Communication channel with native display servers.
 * 2. name space for other TPL objects.
 */
typedef struct _tpl_display tpl_display_t;

/**
 * A structure representing TPL surface object.
 *
 * TPL surface is an object representing an image which can be displayed by the
 * display system. This corresponds to a native surface like X Drawable or
 * wl_surface. A TPL surface might have several TPL buffers on which we can
 * render.
 */
typedef struct _tpl_surface tpl_surface_t;

/**
 * A structure representing TPL buffer object.
 *
 * TPL buffer is an object representing a set of pixels which is usually a
 * block of memories.
 */
typedef struct _tpl_buffer tpl_buffer_t;

/**
 * Function type used for freeing some data.
 */
typedef void (*tpl_free_func_t)(void *data);

/**
 * Object types.
 *
 * @see tpl_object_get_type()
 */
typedef enum
{
	TPL_OBJECT_ERROR = -1,
	TPL_OBJECT_DISPLAY,
	TPL_OBJECT_SURFACE,
	TPL_OBJECT_BUFFER,
	TPL_OBJECT_MAX
} tpl_object_type_t;

/**
 * Surface types.
 *
 * On some display system, there're several types of native surfaces. (ex. X11
 * pixmap and window). Users might want to know what kind of native surface
 * type a TPL surface was made from.
 *
 * @see tpl_surface_create()
 * @see tpl_surface_get_type()
 */
typedef enum
{
	TPL_SURFACE_ERROR = -1,
	TPL_SURFACE_TYPE_WINDOW,	/**< surface gets displayed by the display server. */
	TPL_SURFACE_TYPE_PIXMAP,	/**< surface is an offscreen pixmap. */
	TPL_SURFACE_MAX
} tpl_surface_type_t;

/**
 * Format types.
 *
 * TPL specifies several supportable formats. This can be used to specify the
 * format of a new surface. Each format explains its own depth, color sequence
 * and color bits. and they are enough attributes to convert to the other
 * formats (such as GPU drivers, Windowing systems, Buffer managers).
 *
 * @see tpl_surface_create()
 */
typedef enum
{
	TPL_FORMAT_INVALID = 0,
	TPL_FORMAT_NOT_YET_SPECIFIED = 1,
	TPL_FORMAT_BGR565,
	TPL_FORMAT_RGB565 = 16 + (0 << 8),
	TPL_FORMAT_XBGR8888,
	TPL_FORMAT_XRGB8888 = 32 + (0 << 8),
	TPL_FORMAT_ABGR8888,
	TPL_FORMAT_ARGB8888 = 32 + (1 << 8),
	TPL_FORMAT_BGRA8888,
	TPL_FORMAT_RGBA8888,
	TPL_FORMAT_BGRX8888,
	TPL_FORMAT_RGBX8888,
	TPL_FORMAT_BGR888,
	TPL_FORMAT_RGB888,
	TPL_FORMAT_ABGR4444,
	TPL_FORMAT_ARGB4444,
	TPL_FORMAT_BGRA4444,
	TPL_FORMAT_RGBA4444,
	TPL_FORMAT_ABGR1555,
	TPL_FORMAT_ARGB1555,
	TPL_FORMAT_BGRA5551,
	TPL_FORMAT_RGBA5551,
	TPL_FORMAT_YUV420,
	TPL_FORMAT_NV12,
	TPL_FORMAT_NV21
} tpl_format_t;

#define TPL_FORMAT_GET_DEPTH(format) (int)((format) & 0xFF)

/**
 * Lock usage types.
 *
 * TPL provides buffer locks which are used for synchronization. This usage
 * indicate that what kind of purpose a locking is used for. Depending on the
 * system, multiple read locks might be allowed. Cache might be flushed when
 * CPU access is engaged.
 *
 * @see tpl_buffer_lock()
 */
typedef enum
{
	TPL_LOCK_USAGE_INVALID = 0,
	TPL_LOCK_USAGE_GPU_READ,
	TPL_LOCK_USAGE_GPU_WRITE,
	TPL_LOCK_USAGE_CPU_READ,
	TPL_LOCK_USAGE_CPU_WRITE
} tpl_lock_usage_t;

/**
 * Types of TPL backend.
 *
 * TPL provides platform independent APIs by implementing platform dependent
 * things in a backend. These types represent types of such backends. One of
 * these types should be specified when creating a TPL display object when
 * calling tpl_display_get().
 *
 * @see tpl_display_get()
 * @see tpl_display_get_backend_type()
 */
typedef enum
{
	TPL_BACKEND_ERROR = -1,
	TPL_BACKEND_WAYLAND,
	TPL_BACKEND_GBM,
	TPL_BACKEND_X11_DRI2,
	TPL_BACKEND_X11_DRI3,
	TPL_BACKEND_COUNT,
	TPL_BACKEND_UNKNOWN,
	TPL_BACKEND_MAX
} tpl_backend_type_t;

/**
 * Increase reference count of a TPL object.
 *
 * All TPL objects are reference-counted. They have reference count 1 on
 * creatation. When the reference count drops to 0, the object will be freed.
 *
 * @param object object which will be referenced.
 * @return reference count after reference on success, -1 on error.
 *
 * @see tpl_object_unreference()
 * @see tpl_object_get_reference()
 */
int tpl_object_reference(tpl_object_t *object);

/**
 * Decrease reference count of a TPL object.
 *
 * @param object object which will be unreferenced.
 * @return reference count after unreference on success, -1 on error.
 *
 * @see tpl_object_reference()
 * @see tpl_object_get_reference()
 */
int tpl_object_unreference(tpl_object_t *object);

/**
 * Get reference count of a TPL object.
 *
 * @oaram object object to get reference count.
 * @return reference count on success, -1 on error.
 *
 * @see tpl_object_reference()
 * @see tpl_object_get_reference()
 */
int tpl_object_get_reference(tpl_object_t *object);

/**
 * Get the type of a TPL object.
 *
 * @param object object to get type.
 * @return actual type of the object on success, TPL_OBJECT_ERROR on error.
 */
tpl_object_type_t tpl_object_get_type(tpl_object_t *object);

/**
 * Set user data to a TPL object.
 *
 * Users want to relate some data with a TPL object. This function provides
 * registering a pointer to such data which can be retrieved later using
 * tpl_object_get_user_data().
 *
 * @param object object to set user data to.
 * @param data pointer to the user data.
 * @param free_func free function which is used for freeing the user data when the object is destroyed.
 * @return TPL_TRUE on success, TPL_FALSE on error.
 *
 * @see tpl_object_get_user_data()
 */
tpl_bool_t tpl_object_set_user_data(tpl_object_t *object,
			      void *data,
			      tpl_free_func_t free_func);

/**
 * Get registered user data of a TPL object.
 *
 * @param object object to get user data.
 * @return pointer to the registered user data on success, NULL on error.
 *
 * @see tpl_object_set_user_data()
 */
void * tpl_object_get_user_data(tpl_object_t *object);

tpl_backend_type_t tpl_display_choose_backend_type(tpl_handle_t native_dpy);
/**
 * Create or get TPL display object for the given native display.
 *
 * Create a TPL display if there's no already existing TPL display for the
 * given native display. If given NULL for native_dpy, this function will
 * return default display.
 *
 * @param type backend type of the given native display.
 * @param native_dpy handle to the native display.
 * @return pointer to the display on success, NULL on failure.
 */
tpl_display_t * tpl_display_get(tpl_backend_type_t type,
				tpl_handle_t native_dpy);

/**
 * Bind a client connection(display handle) to the existed TPL display.
 *
 * After bound, The TPL display knows a handle of client connection display and
 * it can recognize client objects (e.g. pixmap surfaces from client
 * application) which were contained by the client connection. So this function
 * must be called by the server process (such as compositor) before using
 * client buffers.
 *
 * @param display display to bind a client connection.
 * @param native_dpy handle of the native client display connection.
 *
 * @see tpl_display_unbind_client_display_handle()
 */
tpl_bool_t tpl_display_bind_client_display_handle(tpl_display_t *display,
						  tpl_handle_t native_dpy);

/**
 * Unbind a client connection(display handle) from the existed TPL display.
 *
 * After being unbound, the TPL display no longer knows about client
 * connection, and all resources from the connection can be unreferenced. If
 * the specified connection was not a bound handle, error occurs.
 *
 * @param display display to unbind a client connection.
 * @param native_dpy handle of the native client display connection.
 *
 * @see tpl_display_bind_client_display_handle()
 */
tpl_bool_t tpl_display_unbind_client_display_handle(tpl_display_t *display,
						    tpl_handle_t native_dpy);

/**
 * Get the backend type of a TPL display.
 *
 * @param display display to get type.
 * @return backend type of the given display.
 *
 * @see tpl_display_get()
 */
tpl_backend_type_t tpl_display_get_backend_type(tpl_display_t *display);

/**
 * Get file descriptor of the buffer manager for the given TPL display.
 *
 * There might be native buffer manager device (ex. DRM). This function exports
 * such native buffer manager for users to be able to access buffers using the
 * buffer manager. How returned buffer manager fd is used is fully dependent on
 * native platform implementation.
 *
 * @param display display to get buffer manger fd.
 * @return file descriptor handle for the buffer manager.
 */
int tpl_display_get_bufmgr_fd(tpl_display_t *display);

/**
 * Get the native display handle which the given TPL display is created for.
 *
 * @param display display to get native handle.
 * @return Handle to the native display.
 *
 * @see tpl_display_get()
 */
tpl_handle_t tpl_display_get_native_handle(tpl_display_t *display);

/**
 * Query supported pixel formats for the given TPL display.
 *
 * Users might want to know what pixel formats are available on the given
 * display. This function is used to query such available pixel formats. Give
 * TPL_DONT_CARE to parameters for size values if any values are acceptable.
 *
 * @param display display to query pixel formats.
 * @param surface_type surface type to query for.
 * @param red_size Size of the red component in bits.
 * @param green_size Size of the green component in bits.
 * @param blue_size Size of the blue component in bits.
 * @param alpha_size Size of the alpha component in bits.
 * @param depth_size Size of a pixel in bits (Color depth).
 * @param native_visual_id Pointer to receive native visual id.
 * @param is_slow Pointer to receive whether the given config is slow.
 * @return TPL_TRUE is the given config is supported, TPL_FALSE otherwise.
 */
tpl_bool_t tpl_display_query_config(tpl_display_t *display,
				    tpl_surface_type_t surface_type,
				    int red_size,
				    int green_size,
				    int blue_size,
				    int alpha_size,
				    int depth_size,
				    int *native_visual_id,
				    tpl_bool_t *is_slow);

/**
 * Filter config according to given TPL display.
 *
 * This function modifies current config specific to the current given TPL
 * display.
 *
 * @param display display to query pixel formats.
 * @param visual_id Pointer to receive native visual id.
 * @param alpha_size Size of the alpha component in bits.
 * @return TPL_TRUE if the given config has been modified, TPL_FALSE otherwise.
 */
tpl_bool_t tpl_display_filter_config(tpl_display_t *display,
				     int *visual_id,
				     int alpha_size);

/**
 * Flush the TPL display.
 *
 * @param display display to flush.
 *
 * There might be pending operations on the given TPL display such as X11
 * native rendering. Flushing TPL display ensures that those pending operations
 * are done.
 */
void tpl_display_flush(tpl_display_t *display);

/**
 * Create a TPL surface for the given native surface.
 *
 * @param display display used for surface creation.
 * @param handle Handle to the native surface.
 * @param type Type of the surface (Window or Pixmap).
 * @param format Pixel format of the surface.
 * @return Created surface on success, NULL otherwise.
 */
tpl_surface_t * tpl_surface_create(tpl_display_t *display,
				   tpl_handle_t handle,
				   tpl_surface_type_t type,
				   tpl_format_t format);

/**
 * Get the TPL display where the given TPL surface was created from.
 *
 * @param surface surface to get display.
 * @return display of the given surface.
 *
 * @see tpl_surface_create()
 */
tpl_display_t * tpl_surface_get_display(tpl_surface_t *surface);

/**
 * Get the native surface handle of the given TPL surface.
 *
 * @param surface surface to get native handle.
 * @return handle to the native surface.
 *
 * @see tpl_surface_create()
 */
tpl_handle_t tpl_surface_get_native_handle(tpl_surface_t *surface);

/**
 * Get the type of the given TPL surface.
 *
 * @param surface surface to get type.
 * @return type of the surface.
 *
 * @see tpl_surface_create()
 */
tpl_surface_type_t tpl_surface_get_type(tpl_surface_t *surface);

/**
 * Get the current size of the given TPL surface.
 *
 * Size of a surface might change when a user resizes window or server resizes
 * it. TPL updates such size information every time when a buffer is queried
 * using tpl_surface_get_buffer(). User have to consider that there might be
 * still mismatch between actual surface size and cached one.
 *
 * @param surface surface to get size.
 * @param width pointer to receive width value.
 * @param height pointer to receive height value.
 */
tpl_bool_t tpl_surface_get_size(tpl_surface_t *surface,
			  int *width,
			  int *height);

/**
 * Begin a new frame of the given TPL surface.
 *
 * TPL forces users to process frames in begin/end manner. Information for a
 * frame such as current buffer can be queried after the frame has begun via
 * tpl_surface_begin_frame(). When a frame ends by calling
 * tpl_surface_end_frame(), the frame is queued. Such queued frames are
 * consumed one by one in FIFO order when tpl_surface_post() is called.
 *
 * If previous frame was not ended, this function will implicitly end previous
 * one.
 *
 * This function has no effect if the surface type is not window.
 *
 * @param surface surface to begin a frame.
 *
 * @see tpl_surface_end_frame()
 * @see tpl_surface_validate_frame()
 * @see tpl_surface_post()
 * @see tpl_surface_get_buffer()
 */
tpl_bool_t tpl_surface_begin_frame(tpl_surface_t *surface);

/**
 * End the current frame of the given TPL surface.
 *
 * The current frame will be queued in the "frame queue". Frame interval and
 * damage region for the frame is copied from surface's current states.
 *
 * This function has no effect if the surface type is not window.
 *
 * @param surface surface to end a frame.
 *
 * @see tpl_surface_begin_frame()
 * @see tpl_surface_validate_frame()
 * @see tpl_surface_post()
 * @see tpl_surface_set_post_interval()
 * @see tpl_surface_set_damage()
 */
tpl_bool_t tpl_surface_end_frame(tpl_surface_t *surface);

/**
 * Validate current frame of the given TPL surface.
 *
 * Users should call this function before getting actual final render target
 * buffer. Calling tpl_surface_get_buffer() after calling this function might
 * give different output with previous one. Buffer returned after calling this
 * function is guaranteed to be not changing. This is somewhat wierd but
 * necessary to fully utilize CPU/GPU in a parallel way on tizen DRI2 protocol
 * implementation (lazy swap).
 *
 * @param surface surface to validate its current frame.
 * @return TPL_TRUE if current buffer is changed due to this validation, TPL_FALSE otherwise.
 *
 * @see tpl_surface_begin_frame()
 * @see tpl_surface_end_frame()
 * @see tpl_surface_get_buffer()
 */
tpl_bool_t tpl_surface_validate_frame(tpl_surface_t *surface);

/**
 * Get the buffer of the current frame for the given TPL surface.
 *
 * This function returns buffer of the current frame. Depending on backend,
 * communication with the server might be required. Returned buffers are used
 * for render target to draw current frame.
 *
 * Returned buffers are valid until next tpl_surface_get_buffer() returns
 * TPL_FALSE on reset_buffers parameter. If TPL_TRUE is returned on
 * reset_buffers, all previsouly returned buffers should no longer be used.
 *
 * @param surface surface to get buffer for the current frame.
 * @param reset_buffers pointer to receive whether previouly returned buffers should be reset or not.
 * @return buffer for the current frame.
 *
 * Calling this function multiple times within a single frame is not guranteed
 * to return a same buffer.
 *
 * @see tpl_surface_begin_frame()
 * @see tpl_surface_end_frame()
 */
tpl_buffer_t * tpl_surface_get_buffer(tpl_surface_t *surface,
				      tpl_bool_t *reset_buffers);

/**
 * Post a frame from the frame queue of the given surface.
 *
 * This function request display server to post a frame. This is the only
 * function which can dequeue a frame from the frame queue.
 *
 * Make sure this function is called exactly once for a frame. Depending on
 * backend, other TPL functions might wait for this function to be called.
 * Scheduling post calls on a separate thread is recommended.
 *
 * If tpl_surface_end_frame() was not called for the current frame, this
 * function might implicitly end the current frame.
 *
 * @param surface surface to post a frame.
 *
 * @see tpl_surface_begin_frame()
 * @see tpl_surface_end_frame()
 */
tpl_bool_t tpl_surface_post(tpl_surface_t *surface);

/**
 * Set frame interval of the given TPL surface.
 *
 * Frame interval ensures that only a single frame is posted within the
 * specified vsync intervals. When a frame ends, the frame's interval is set to
 * the surface's current interval.
 *
 * @param surface surface to set frame interval.
 * @param interval minimum number of vsync between frames.
 *
 * @see tpl_surface_get_post_interval()
 */
tpl_bool_t tpl_surface_set_post_interval(tpl_surface_t *surface,
				   int interval);

/**
 * Get frame interval of the given TPL surface.
 *
 * @param surface surface to get frame interval.
 * @return frame interval.
 *
 * @see tpl_surface_set_post_interval()
 */
int tpl_surface_get_post_interval(tpl_surface_t *surface);

/**
 * Set damaged region of the given TPL surface.
 *
 * Damage information is used for reducing number of pixels composited in the
 * compositor. When a frame ends, the frames' damage area is copied from the
 * surface's current damage region. Setting num_rects to 0 or rects to NULL
 * means entire area is damaged.
 *
 * @param surface surface to set damage region.
 * @param num_rects number of rectangles of the damage region.
 * @param rects pointer to coordinates of rectangles. x0, y0, w0, h0, x1, y1, w1, h1...
 *
 * @see tpl_surface_get_damage()
 */
tpl_bool_t tpl_surface_set_damage(tpl_surface_t *surface,
			    int num_rects,
			    const int *rects);

/**
 * Get damaged region of the given TPL surface.
 *
 * @param surface surface to get damage region.
 * @param num_rects Pointer to receive the number of rectangles.
 * @param rects Pointer to receive the pointer to rectangle coordinate array.
 *
 * @see tpl_surface_set_damage()
 */
tpl_bool_t tpl_surface_get_damage(tpl_surface_t *surface,
			    int *num_rects,
			    const int **rects);

tpl_bool_t tpl_surface_destroy_cached_buffers(tpl_surface_t *surface);

tpl_bool_t tpl_surface_update_cached_buffers(tpl_surface_t *surface);

/**
 * Map the given buffer to the user space address.
 *
 * Users can do CPU access to the buffer memory by using this function. It is
 * recommended to lock the buffer first with appropriate lock usage to avoid
 * cache coherency problems.
 *
 * @param buffer buffer to map.
 * @param size Size of the area to be mapped. Give 0 for entire buffer.
 * @return Pointer to the mapped buffer memory.
 *
 * @see tpl_buffer_unmap()
 */
void * tpl_buffer_map(tpl_buffer_t *buffer,
		      int size);

/**
 * Unmap the given buffer from the user space address.
 *
 * @param buffer buffer to unmap
 * @param ptr Pointer to the mapped memory. Give NULL if the entire buffer was mapped.
 * @param size Size of the mapped memory. Give 0 if the entire buffer was mapped.
 *
 * @see tpl_buffer_map()
 */
void tpl_buffer_unmap(tpl_buffer_t *buffer,
		      void *ptr,
		      int size);

/**
 * Lock the given buffer.
 *
 * Buffer lock is used for synchronizations. The locking is actually done to
 * the low-level buffer object like dma_buf. So, it is possible that locking
 * call is blocked although no locking is ever called for the TPL buffer. Other
 * TPL buffer pointing to the same low-level buffer might be locked or other
 * process might be holding the lock for the same low-level buffer.
 *
 * The lock might work as R/W lock depending on backend.
 *
 * @param buffer buffer to lock.
 * @param usage purpose of the lock.
 * @return TPL_TRUE on success, TPL_FALSE otherwise.
 *
 * @see tpl_buffer_unlock()
 */
tpl_bool_t tpl_buffer_lock(tpl_buffer_t *buffer,
			   tpl_lock_usage_t usage);

/**
 * Unlock the given buffer.
 *
 * @param buffer buffer to unlock.
 *
 * @see tpl_buffer_lock()
 */
void tpl_buffer_unlock(tpl_buffer_t *buffer);

/**
 * Create a native buffer of the given TPL buffer.
 *
 * This function can be used to export a TPL buffer by the returned native
 * buffer. Some windowing system need a extra buffer export mechanism between
 * compositor and application.
 *
 * @param buffer buffer to export.
 * @return A native buffer from the buffer. NULL on error.
 */
void *tpl_buffer_create_native_buffer(tpl_buffer_t *buffer);

/**
 * Get the low-level buffer key of the given TPL buffer.
 *
 * It is a common method representing buffers with 32bits or 64bits key. A TPL
 * buffer internally indicate a platform dependent low-level buffer like
 * dma_buf. This function retrieves such key to the low-level buffer.
 *
 * @param buffer buffer to retrieve the key.
 * @return Key to the low-level buffer.
 *
 * @see tpl_buffer_get_fd()
 */
size_t tpl_buffer_get_key(tpl_buffer_t *buffer);

/**
 * Get the low-level buffer fd of the given TPL buffer.
 *
 * It is also a common method accessing a buffer via file system. This function
 * returns file descriptor for the low-level buffer.
 *
 * @param buffer buffer to retrieve fd.
 * @return file descriptor of the low-level buffer.
 *
 * @see tpl_buffer_get_key()
 */
int tpl_buffer_get_fd(tpl_buffer_t *buffer);

/**
 * Get the age of the given TPL buffer.
 *
 * Buffer age gives us information on content which is already rendered on the
 * buffer. It is used to do partial update which is an optimization techinique
 * that renders only different area between current frame and previously
 * rendered buffer content.
 *
 * @param buffer buffer to get age.
 * @return age of the buffer..
 */
int tpl_buffer_get_age(tpl_buffer_t *buffer);

/**
 * Get the TPL surface where the given TPL buffer belongs to.
 *
 * @param buffer buffer to get the belonging surface.
 * @return surface where the given buffer belongs to.
 *
 * @see tpl_surface_get_buffer()
 */
tpl_surface_t * tpl_buffer_get_surface(tpl_buffer_t *buffer);

/**
 * Get the size of the given TPL buffer.
 *
 * @param buffer buffer to get the size.
 * @param width pointer to receive the width value.
 * @param height pointer to receive the height value.
 * @return TPL_TRUE on success, TPL_FALSE on error.
 */
tpl_bool_t tpl_buffer_get_size(tpl_buffer_t *buffer,
			 int *width,
			 int *height);

/**
 * Get the color depth of the given TPL buffer.
 *
 * @param buffer buffer to get the color depth.
 * @return color depth of the given buffer.
 */
int tpl_buffer_get_depth(tpl_buffer_t *buffer);

/**
 * Get the pitch value (in bytes) of the given TPL buffer.
 *
 * @param buffer buffer to get the pitch.
 * @return pitch of the given buffer in bytes.
 */
int tpl_buffer_get_pitch(tpl_buffer_t *buffer);

int tpl_buffer_get_map_cnt(tpl_buffer_t *buffer);
/**
 * Get the ID of the given TPL buffer.
 *
 * @param buffer buffer to get the id.
 * @return id of the given buffer.
 */

unsigned int tpl_buffer_get_id(tpl_buffer_t *buffer);


int tpl_buffer_get_map_cnt(tpl_buffer_t *buffer);

/**
 * Query information on the given native window.
 *
 * @param display display used for query.
 * @param window handle to the native window.
 * @param width pointer to receive width of the window.
 * @param height pointer to receive height of the window.
 * @param format pointer to receive format of the window.
 * @return TPL_TRUE if the window is valid, TPL_FALSE otherwise.
 */
tpl_bool_t tpl_get_native_window_info(tpl_display_t *display,
				      tpl_handle_t window,
				      int *width,
				      int *height,
				      tpl_format_t *format,
				      int depth,
				      int a_size);

/**
 * Query information on the given native pixmap.
 *
 * @param display display used for query.
 * @param pixmap handle to the native pixmap.
 * @param width pointer to receive width of the pixmap.
 * @param height pointer to receive height of the pixmap.
 * @param format pointer to receive format of the pixmap.
 * @return TPL_TRUE if the pixmap is valid, TPL_FALSE otherwise.
 */
tpl_bool_t tpl_get_native_pixmap_info(tpl_display_t *display,
				      tpl_handle_t pixmap,
				      int *width,
				      int *height,
				      tpl_format_t *format);

void tpl_display_wait_native(tpl_display_t *display);

#endif /* TPL_H */
