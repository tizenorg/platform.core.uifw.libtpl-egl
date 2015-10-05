#include <string.h>
#include "tpl_internal.h"

void
__tpl_region_init(tpl_region_t *region)
{
	TPL_ASSERT(region);

	region->num_rects = 0;

	/* tpl_region_t will initially provide TPL_MIN_REGION_RECTS number of
	   storage space for rects after which heap memory will be allocated
	   if the number of required space exceeds TPL_MIN_REGION_RECTS
	*/
	region->rects = &region->rects_static[0];
	region->num_rects_allocated = TPL_MIN_REGION_RECTS;

	TPL_LOG(3, "region:%p {%d, %p, %p, %d}", region, region->num_rects, region->rects,
		&region->rects_static[0], region->num_rects_allocated);
}

void
__tpl_region_fini(tpl_region_t *region)
{
	TPL_ASSERT(region);
	TPL_ASSERT(region->rects);

	TPL_LOG(3, "region:%p {%d, %p, %p, %d}", region, region->num_rects, region->rects,
		&region->rects_static[0], region->num_rects_allocated);

	if (region->rects != &region->rects_static[0])
		free(region->rects);
}

tpl_region_t *
__tpl_region_alloc()
{
	tpl_region_t *region;

	region = (tpl_region_t *) calloc(1, sizeof(tpl_region_t));
	if (NULL == region)
		return NULL;

	__tpl_region_init(region);

	return region;
}

void
__tpl_region_free(tpl_region_t **region)
{
	TPL_ASSERT(region);
	TPL_ASSERT(*region);

	__tpl_region_fini(*region);
	free(*region);

	*region = NULL;
}

tpl_bool_t
__tpl_region_is_empty(const tpl_region_t *region)
{
	TPL_ASSERT(region);

	TPL_LOG(3, "region:%p {%d, %p, %p, %d}", region, region->num_rects,
		region->rects, &region->rects_static[0], region->num_rects_allocated);

	return (region->num_rects == 0);
}

tpl_bool_t
__tpl_region_copy(tpl_region_t *dst, const tpl_region_t *src)
{
	TPL_ASSERT(src);
	TPL_ASSERT(dst);

	return __tpl_region_set_rects(dst, src->num_rects, src->rects);
}

tpl_bool_t
__tpl_region_set_rects(tpl_region_t *region, int num_rects, const int *rects)
{
	TPL_ASSERT(region);
	TPL_ASSERT(num_rects >= 0);

	TPL_LOG(3, "region:%p {%d, %p, %p, %d}, num_rects:%d, rects:%p", region,
		region->num_rects, region->rects, &region->rects_static[0],
		region->num_rects_allocated, num_rects, rects);

	/* allocate memory if the number of rects exceed the allocated memory */
	if (num_rects > region->num_rects_allocated)
	{
		if (region->rects != &region->rects_static[0])
			free(region->rects);

		region->rects = (int *) malloc(num_rects * 4 * sizeof(int));
		if (NULL == region->rects)
			return TPL_FALSE;

		region->num_rects_allocated = num_rects;
	}

	region->num_rects = num_rects;

	if (0 < num_rects)
		memcpy(region->rects, rects, num_rects * 4 * sizeof(int));

	return TPL_TRUE;
}
