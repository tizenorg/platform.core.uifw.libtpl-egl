#include "tpl_internal.h"

tpl_frame_t *
__tpl_frame_alloc()
{
	tpl_frame_t *frame;

	frame = (tpl_frame_t *)calloc(1, sizeof(tpl_frame_t));

	__tpl_region_init(&frame->damage);

	return frame;
}



#if TPL_WINSYS_WL
void
__tpl_frame_free(tpl_frame_t *frame)
{
	TPL_ASSERT(frame);

	if (frame->tbm_surface)
		tbm_surface_internal_unref(frame->tbm_surface);

	__tpl_region_fini(&frame->damage);
	free(frame);
}

void
__tpl_frame_set_buffer(tpl_frame_t *frame, tbm_surface_h tbm_surface)
{
	TPL_ASSERT(frame);
	TPL_ASSERT(tbm_surface);

	if (frame->tbm_surface)
		tbm_surface_internal_unref(frame->tbm_surface);

	tbm_surface_internal_ref(tbm_surface);
	frame->tbm_surface = tbm_surface;
}
#else
void
__tpl_frame_free(tpl_frame_t *frame)
{
	TPL_ASSERT(frame);

	if (frame->buffer)
		tpl_object_unreference((tpl_object_t *)frame->buffer);

	__tpl_region_fini(&frame->damage);
	free(frame);
}

void
__tpl_frame_set_buffer(tpl_frame_t *frame, tpl_buffer_t *buffer)
{
	TPL_ASSERT(frame);
	TPL_ASSERT(buffer);

	if (frame->buffer)
		tpl_object_unreference((tpl_object_t *)frame->buffer);

	tpl_object_reference((tpl_object_t *)buffer);
	frame->buffer = buffer;
}
#endif
