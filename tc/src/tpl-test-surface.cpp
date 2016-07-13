/**
 * @file tpl-test-surface.cpp
 * @brief TPL Test case of surface 
 *
 */ 

#include "gtest/gtest.h"

#include "tpl-test.h"


class TPLSurfaceTest : public TPLTestBase {};


TEST_F(TPLSurfaceTest, tpl_surface_validate)
{
	tpl_bool_t result = tpl_surface_validate(this->backend->tpl_surface);
	ASSERT_EQ(TPL_TRUE, result);
}

TEST_F(TPLSurfaceTest, tpl_surface_get_args)
{
	// tpl_surface_get_display test
	tpl_display_t* test_dpy = tpl_surface_get_display(this->backend->tpl_surface);
	ASSERT_NE((void *)NULL, test_dpy);

	// tpl_surface_get_native_handle
	tpl_handle_t test_handle = tpl_surface_get_native_handle(
							  this->backend->tpl_surface);
	ASSERT_NE((void *)NULL, test_handle);

	// tpl_surface_get_type test
	tpl_surface_type_t test_type = tpl_surface_get_type(
								  this->backend->tpl_surface);
	ASSERT_EQ(TPL_SURFACE_TYPE_WINDOW, test_type);

	// tpl_surface_get_size test
	int width, height;
	tpl_result_t test_size = tpl_surface_get_size(this->backend->tpl_surface,
							&width, &height);
	EXPECT_EQ(config.width, width);
	EXPECT_EQ(config.height, height);
	ASSERT_EQ(TPL_ERROR_NONE, test_size);
}

TEST_F(TPLSurfaceTest, tpl_surface_dequeue_and_enqueue_buffer_test)
{
	tbm_surface_h tbm_surf = NULL;
	tbm_surf = tpl_surface_dequeue_buffer(this->backend->tpl_surface);
	ASSERT_NE((void *)NULL, tbm_surf);

	int interval_set = 2;
	tpl_surface_set_post_interval(this->backend->tpl_surface, interval_set);
	int interval_get = tpl_surface_get_post_interval(
					  this->backend->tpl_surface);
	ASSERT_EQ(interval_set, interval_get);

	tpl_result_t result = TPL_ERROR_INVALID_PARAMETER; 
	result = tpl_surface_enqueue_buffer(this->backend->tpl_surface, tbm_surf);
	ASSERT_EQ(TPL_ERROR_NONE, result);
}

