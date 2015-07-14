#include "tpl_internal.h"
#include <pthread.h>

tpl_bool_t
__tpl_object_is_valid(tpl_object_t *object)
{
	return tpl_util_osu_atomic_get(&object->reference) != 0;
}

void
__tpl_object_init(tpl_object_t *object, tpl_object_type_t type, tpl_free_func_t free_func)
{
	object->type = type;
	object->free = free_func;
	tpl_util_osu_atomic_set(&object->reference, 1);
	pthread_mutex_init(&object->mutex, NULL);
}

void
__tpl_object_fini(tpl_object_t *object)
{
	pthread_mutex_destroy(&object->mutex);

	if (object->user_data.free)
		object->user_data.free(object->user_data.data);
}

void
__tpl_object_lock(tpl_object_t *object)
{
	TPL_ASSERT(__tpl_object_is_valid(object));
	pthread_mutex_lock(&object->mutex);
}

void
__tpl_object_unlock(tpl_object_t *object)
{
	TPL_ASSERT(__tpl_object_is_valid(object));
	pthread_mutex_unlock(&object->mutex);
}

int
tpl_object_reference(tpl_object_t *object)
{
	TPL_ASSERT(__tpl_object_is_valid(object));
	return (int) tpl_util_osu_atomic_inc(&object->reference);
}

int
tpl_object_unreference(tpl_object_t *object)
{
	int reference;

	TPL_ASSERT(__tpl_object_is_valid(object));

	reference = (int)tpl_util_osu_atomic_dec(&object->reference);

	if (reference == 0)
	{
		__tpl_object_fini(object);
		object->free(object);
	}

	return reference;
}

int
tpl_object_get_reference(tpl_object_t *object)
{
	TPL_ASSERT(__tpl_object_is_valid(object));
	return (int)tpl_util_osu_atomic_get(&object->reference);
}

tpl_object_type_t
tpl_object_get_type(tpl_object_t *object)
{
	TPL_ASSERT(__tpl_object_is_valid(object));
	return object->type;
}

void
tpl_object_set_user_data(tpl_object_t *object, void *data, tpl_free_func_t free_func)
{
	TPL_ASSERT(__tpl_object_is_valid(object));

	__tpl_object_lock(object);
	object->user_data.data = data;
	object->user_data.free = free_func;
	__tpl_object_unlock(object);
}

void *
tpl_object_get_user_data(tpl_object_t *object)
{
	void *data;

	TPL_ASSERT(__tpl_object_is_valid(object));
	__tpl_object_lock(object);
	data = object->user_data.data;
	__tpl_object_unlock(object);

	return data;
}
