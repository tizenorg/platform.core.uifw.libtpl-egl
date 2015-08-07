#include "tpl_internal.h"
#include <pthread.h>

tpl_bool_t
__tpl_object_is_valid(tpl_object_t *object)
{
	if (NULL == object)
		return TPL_FALSE;

	return (0 != __tpl_util_atomic_get(&object->reference));
}

tpl_bool_t
__tpl_object_init(tpl_object_t *object, tpl_object_type_t type, tpl_free_func_t free_func)
{
	TPL_ASSERT(object);
	TPL_ASSERT(type >= 0 && type < TPL_OBJECT_MAX);

	object->type = type;
	object->free = free_func;

	__tpl_util_atomic_set(&object->reference, 1);

	if (0 != pthread_mutex_init(&object->mutex, NULL))
		return TPL_FALSE;

	return TPL_TRUE;
}

tpl_bool_t
__tpl_object_fini(tpl_object_t *object)
{
	TPL_ASSERT(object);

	if (0 != pthread_mutex_destroy(&object->mutex))
		return TPL_FALSE;

	if (object->user_data.free)
		object->user_data.free(object->user_data.data);

	return TPL_TRUE;
}

tpl_bool_t
__tpl_object_lock(tpl_object_t *object)
{
	TPL_ASSERT(object);

	if (0 != pthread_mutex_lock(&object->mutex))
		return TPL_FALSE;

	return TPL_TRUE;
}

void
__tpl_object_unlock(tpl_object_t *object)
{
	TPL_ASSERT(object);

	pthread_mutex_unlock(&object->mutex);
}

int
tpl_object_reference(tpl_object_t *object)
{
	if (TPL_TRUE != __tpl_object_is_valid(object))
	{
		TPL_ERR("input object is invalid!");
		return -1;
	}

	return (int) __tpl_util_atomic_inc(&object->reference);
}

int
tpl_object_unreference(tpl_object_t *object)
{
	unsigned int reference;

	if (TPL_TRUE != __tpl_object_is_valid(object))
	{
		TPL_ERR("input object is invalid!");
		return -1;
	}

	reference = __tpl_util_atomic_dec(&object->reference);

	if (0 == reference)
	{
		__tpl_object_fini(object);
		object->free(object);
	}

	return (int) reference;
}

int
tpl_object_get_reference(tpl_object_t *object)
{
	if (TPL_TRUE != __tpl_object_is_valid(object))
	{
		TPL_ERR("input object is invalid!");
		return -1;
	}

	return (int) __tpl_util_atomic_get(&object->reference);
}

tpl_object_type_t
tpl_object_get_type(tpl_object_t *object)
{
	if (TPL_TRUE != __tpl_object_is_valid(object))
	{
		TPL_ERR("input object is invalid!");
		return TPL_OBJECT_ERROR;
	}

	return object->type;
}

tpl_bool_t
tpl_object_set_user_data(tpl_object_t *object, void *data, tpl_free_func_t free_func)
{
	if (TPL_TRUE != __tpl_object_is_valid(object))
	{
		TPL_ERR("input object is invalid!");
		return TPL_FALSE;
	}

	__tpl_object_lock(object);
	object->user_data.data = data;
	object->user_data.free = free_func;
	__tpl_object_unlock(object);

	return TPL_TRUE;
}

void *
tpl_object_get_user_data(tpl_object_t *object)
{
	void *data;

	if (TPL_TRUE != __tpl_object_is_valid(object))
	{
		TPL_ERR("input object is invalid!");
		return NULL;
	}

	__tpl_object_lock(object);
	data = object->user_data.data;
	__tpl_object_unlock(object);

	return data;
}
