#include "tpl_internal.h"

tpl_frame_t *
__tpl_frame_alloc()
{
	tpl_frame_t *frame;

	frame = (tpl_frame_t *)calloc(1, sizeof(tpl_frame_t));

	__tpl_region_init(&frame->damage);

	return frame;
}

void
__tpl_frame_free(tpl_frame_t *frame)
{
	int ref;

	TPL_ASSERT(frame);

	if (frame->buffer)
	{
		ref = tpl_object_unreference((tpl_object_t *)frame->buffer);
		TPL_LOG(9, "---------- buffer:%p | name:%d | ref:%d", frame->buffer, frame->buffer->key, ref);
	}
	__tpl_region_fini(&frame->damage);
	free(frame);
}

void
__tpl_frame_set_buffer(tpl_frame_t *frame, tpl_buffer_t *buffer)
{
	TPL_ASSERT(frame);
	TPL_ASSERT(buffer);

	int ref;

	if (frame->buffer)
	{
		tpl_object_unreference((tpl_object_t *)frame->buffer);
		TPL_LOG(9, "---------- buffer:%p | name:%d | ref:%d", frame->buffer, frame->buffer->key, ref);
	}

	ref = tpl_object_reference((tpl_object_t *)buffer);
	TPL_LOG(9, "++++++++++ buffer:%p | name:%d | ref:%d", buffer, buffer->key, ref);
	frame->buffer = buffer;
}
