#include "tpl_internal.h"

tpl_frame_t *
__tpl_frame_alloc()
{
	tpl_frame_t *frame;

	frame = (tpl_frame_t *)calloc(1, sizeof(tpl_frame_t));
	TPL_ASSERT(frame != NULL);

	return frame;
}

void
__tpl_frame_free(tpl_frame_t *frame)
{
	if (frame->buffer)
		tpl_object_unreference((tpl_object_t *)frame->buffer);

	tpl_region_fini(&frame->damage);
	free(frame);
}

void
__tpl_frame_set_buffer(tpl_frame_t *frame, tpl_buffer_t *buffer)
{
	if (frame->buffer)
		tpl_object_unreference((tpl_object_t *)frame->buffer);

	tpl_object_reference((tpl_object_t *)buffer);
	frame->buffer = buffer;
}
