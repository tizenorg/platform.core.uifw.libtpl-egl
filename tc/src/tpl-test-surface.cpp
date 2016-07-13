/**
 * @file tpl-test-surface.cpp
 * @brief TPL Test case of surface 
 *
 */ 

#include "tpl-test.h"

#include "gtest/gtest.h"


TEST_F(TPLTest, tpl_surface_validate)
{
	tpl_bool_t result = tpl_surface_validate(tpl_surface);
	ASSERT_EQ(TPL_TRUE, result);
}

TEST_F(TPLTest, tpl_surface_get_args)
{
	// tpl_surface_get_display test
	tpl_display_t* test_dpy = tpl_surface_get_display(tpl_surface);
	ASSERT_NE((void *)NULL, test_dpy);

	// tpl_surface_get_native_handle
	tpl_handle_t test_handle = tpl_surface_get_native_handle(tpl_surface);
	ASSERT_NE((void *)NULL, test_handle);

	// tpl_surface_get_type test
	tpl_surface_type_t test_type = tpl_surface_get_type(tpl_surface);
	ASSERT_EQ(TPL_SURFACE_TYPE_WINDOW, test_type);

	// tpl_surface_get_size test
	int width, height;
	tpl_result_t test_size = tpl_surface_get_size(tpl_surface, &width, &height);
	EXPECT_EQ(config->width, width);
	EXPECT_EQ(config->height, height);
	ASSERT_EQ(TPL_ERROR_NONE, test_size);
}

TEST_F(TPLTest, tpl_surface_dequeue_and_enqueue_buffer_test)
{
	tbm_surface_h tbm_surf = NULL;
	tbm_surf = tpl_surface_dequeue_buffer(tpl_surface);
	ASSERT_NE((void *)NULL, tbm_surf);

	int interval_set = 2;
	tpl_surface_set_post_interval(tpl_surface, interval_set);
	int interval_get = tpl_surface_get_post_interval(tpl_surface);
	ASSERT_EQ(interval_set, interval_get);

	tpl_result_t result = TPL_ERROR_INVALID_PARAMETER; 
	result = tpl_surface_enqueue_buffer(tpl_surface, tbm_surf);
	ASSERT_EQ(TPL_ERROR_NONE, result);

}
