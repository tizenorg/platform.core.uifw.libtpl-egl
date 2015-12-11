#include "tpl_internal.h"

static void
__tpl_buffer_fini(tpl_buffer_t *buffer)
{
	TPL_ASSERT(buffer);
	TPL_ASSERT(buffer->backend.fini);

	buffer->backend.fini(buffer);
}

static void
__tpl_buffer_free(void *buffer)
{
	TPL_ASSERT(buffer);

	__tpl_buffer_fini((tpl_buffer_t *) buffer);
	free(buffer);
}

tpl_buffer_t *
__tpl_buffer_alloc(tpl_surface_t *surface, size_t key, int fd, int width, int height,
		   int depth, int pitch)
{
	tpl_buffer_t *buffer;
	tpl_bool_t ret;

	TPL_ASSERT(surface);
	TPL_ASSERT(surface->display);

	buffer = (tpl_buffer_t *)calloc(1, sizeof(tpl_buffer_t));
	if (NULL == buffer)
		return NULL;

	ret = __tpl_object_init(&buffer->base, TPL_OBJECT_BUFFER, __tpl_buffer_free);
	if (TPL_TRUE != ret)
		return NULL;

	buffer->surface = surface;
	buffer->key = key;
	buffer->fd = fd;
	buffer->age = -1;

	buffer->width = width;
	buffer->height = height;
	buffer->depth = depth;
	buffer->pitch = pitch;
	buffer->map_cnt = 0;

	TPL_LOG(9, "++++++++++ buffer:%p | name:%d | ref:%d", buffer, key, 1);

	/* Backend initialization. */
	__tpl_buffer_init_backend(buffer, surface->display->backend.type);

	if (TPL_TRUE != buffer->backend.init(buffer))
	{
		tpl_object_unreference((tpl_object_t *) buffer);
		return NULL;
	}

	TPL_LOG(3, "buffer(%p) surface(%p, %p) key:%zu fd:%d %dx%d", (void *) buffer, surface, surface->native_handle, key, fd, width, height);
	return buffer;
}

void
__tpl_buffer_set_surface(tpl_buffer_t *buffer, tpl_surface_t *surface)
{
	TPL_ASSERT(buffer);

	buffer->surface = surface;
}

void *
tpl_buffer_map(tpl_buffer_t *buffer, int size)
{
	void *ptr;

	if (NULL == buffer)
	{
		TPL_ERR("buffer is NULL!");
		return NULL;
	}

	TPL_OBJECT_LOCK(buffer);
	ptr = buffer->backend.map(buffer, size);
	TPL_OBJECT_UNLOCK(buffer);

	return ptr;
}

void
tpl_buffer_unmap(tpl_buffer_t *buffer, void *ptr, int size)
{
	if (NULL == buffer)
	{
		TPL_ERR("buffer is NULL!");
		return;
	}

	TPL_OBJECT_LOCK(buffer);
	buffer->backend.unmap(buffer, ptr, size);
	TPL_OBJECT_UNLOCK(buffer);
}

tpl_bool_t
tpl_buffer_lock(tpl_buffer_t *buffer, tpl_lock_usage_t usage)
{
	tpl_bool_t result;

	if (NULL == buffer)
	{
		TPL_ERR("buffer is NULL!");
		return TPL_FALSE;
	}

	TPL_OBJECT_LOCK(buffer);
	result = buffer->backend.lock(buffer, usage);
	buffer->map_cnt++;
	TPL_OBJECT_UNLOCK(buffer);

	return result;
}

void
tpl_buffer_unlock(tpl_buffer_t *buffer)
{
	if (NULL == buffer)
	{
		TPL_ERR("buffer is NULL!");
		return;
	}

	TPL_OBJECT_LOCK(buffer);
	buffer->backend.unlock(buffer);
	buffer->map_cnt--;
	TPL_OBJECT_UNLOCK(buffer);
}

size_t
tpl_buffer_get_key(tpl_buffer_t *buffer)
{
	if (NULL == buffer)
	{
		TPL_ERR("buffer is NULL!");
		return -1;
	}

	return buffer->key;
}

int
tpl_buffer_get_fd(tpl_buffer_t *buffer)
{
	if (NULL == buffer)
	{
		TPL_ERR("buffer is NULL!");
		return -1;
	}

	return buffer->fd;
}

int
tpl_buffer_get_age(tpl_buffer_t *buffer)
{
	int age;

	if (NULL == buffer)
	{
		TPL_ERR("buffer is NULL!");
		return -1;
	}

	TPL_OBJECT_LOCK(buffer);

	/* Get buffer age from TPL */
	if (buffer->backend.get_buffer_age != NULL)
		age = buffer->backend.get_buffer_age(buffer);
	else
		age = buffer->age;

	TPL_OBJECT_UNLOCK(buffer);

	return age;
}

tpl_surface_t *
tpl_buffer_get_surface(tpl_buffer_t *buffer)
{
	if (NULL == buffer)
	{
		TPL_ERR("buffer is NULL!");
		return NULL;
	}

	return buffer->surface;
}

tpl_bool_t
tpl_buffer_get_size(tpl_buffer_t *buffer, int *width, int *height)
{
	if (NULL == buffer)
	{
		TPL_ERR("buffer is NULL!");
		return TPL_FALSE;
	}

	if (width)
		*width = buffer->width;

	if (height)
		*height = buffer->height;

	return TPL_TRUE;
}

int
tpl_buffer_get_depth(tpl_buffer_t *buffer)
{
	if (NULL == buffer)
	{
		TPL_ERR("buffer is NULL!");
		return -1;
	}

	return buffer->depth;
}

int
tpl_buffer_get_pitch(tpl_buffer_t *buffer)
{
	if (NULL == buffer)
	{
		TPL_ERR("buffer is NULL!");
		return -1;
	}

	return buffer->pitch;
}

int
tpl_buffer_get_map_cnt(tpl_buffer_t *buffer)
{
	return buffer->map_cnt;
}
void *
tpl_buffer_create_native_buffer(tpl_buffer_t *buffer)
{
	if (NULL == buffer)
	{
		TPL_ERR("buffer is NULL!");
		return NULL;
	}

	if (NULL == buffer->backend.create_native_buffer)
		return NULL;

	return buffer->backend.create_native_buffer(buffer);
}
