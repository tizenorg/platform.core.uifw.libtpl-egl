#include "tpl_internal.h"
#include <pthread.h>

tpl_bool_t
__tpl_object_is_valid(tpl_object_t *object)
{
	if (NULL == object)
		return TPL_FALSE;

	return (0 != __tpl_util_atomic_get(&object->reference));
}

tpl_result_t
__tpl_object_init(tpl_object_t *object, tpl_object_type_t type, tpl_free_func_t free_func)
{
	TPL_ASSERT(object);
	TPL_ASSERT(type >= 0 && type < TPL_OBJECT_MAX);

	object->type = type;
	object->free = free_func;
	tpl_util_map_pointer_init(&object->user_data_map, TPL_OBJECT_BUCKET_BITS, &object->buckets[0]);

	__tpl_util_atomic_set(&object->reference, 1);

	if (0 != pthread_mutex_init(&object->mutex, NULL))
	{
		TPL_ERR("tpl_object_t pthread_mutex_init failed.");
		return TPL_ERROR_INVALID_OPERATION;
	}

	return TPL_ERROR_NONE;
}

tpl_result_t
__tpl_object_fini(tpl_object_t *object)
{
	TPL_ASSERT(object);

	if (0 != pthread_mutex_destroy(&object->mutex))
	{
		TPL_ERR("tpl_object_t pthread_mutex_destroy failed.");
		return TPL_ERROR_INVALID_OPERATION;
	}

	tpl_util_map_fini(&object->user_data_map);

	return TPL_ERROR_NONE;
}

tpl_result_t
__tpl_object_lock(tpl_object_t *object)
{
	TPL_ASSERT(object);

	if (0 != pthread_mutex_lock(&object->mutex))
	{
		TPL_ERR("tpl_object_t pthread_mutex_lock failed.");
		return TPL_ERROR_INVALID_OPERATION;
	}
	return TPL_ERROR_NONE;
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

tpl_result_t
tpl_object_set_user_data(tpl_object_t *object, void *key, void *data, tpl_free_func_t free_func)
{
	tpl_util_key_t _key;

	if (TPL_TRUE != __tpl_object_is_valid(object))
	{
		TPL_ERR("input object is invalid!");
		return TPL_ERROR_INVALID_PARAMETER;
	}

	__tpl_object_lock(object);
	_key.ptr = key;
	tpl_util_map_set(&object->user_data_map, _key, data, free_func);
	__tpl_object_unlock(object);

	return TPL_ERROR_NONE;
}

void *
tpl_object_get_user_data(tpl_object_t *object, void *key)
{
	tpl_util_key_t _key;
	void *data;

	if (TPL_TRUE != __tpl_object_is_valid(object))
	{
		TPL_ERR("input object is invalid!");
		return NULL;
	}

	__tpl_object_lock(object);
	_key.ptr = key;
	data = tpl_util_map_get(&object->user_data_map, _key);
	__tpl_object_unlock(object);

	return data;
}
