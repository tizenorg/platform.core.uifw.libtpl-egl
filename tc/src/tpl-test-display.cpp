/**
 * @file tpl-test-display.cpp
 * @brief TPL Test case of display 
 *
 */ 

#include "gtest/gtest.h"

#include "tpl-test.h"


class TPLDisplayTest : public TPLTestBase {};
class TPLDisplayTestIfSupported : public TPLDisplayTest {};


TEST_F(TPLDisplayTest, tpl_display_get)
{
	ASSERT_NE((void *)NULL, tpl_display_get(this->backend->display_handle));
}


TEST_F(TPLDisplayTest, tpl_display_get_native_handle)
{
	ASSERT_NE((void *)NULL,
			  tpl_display_get_native_handle(this->backend->tpl_display));
}


TEST_F(TPLDisplayTest, tpl_display_query_config)
{
	tpl_result_t result;

	// #1: Normal case
	result = tpl_display_query_config(this->backend->tpl_display,
			TPL_SURFACE_TYPE_WINDOW,
			8,		// red size
			8,		// green size
			8,		// blue size
			8,		// alpha size
			32,		// depth size
			NULL,
			NULL);

	ASSERT_EQ(TPL_ERROR_NONE, result);
   
	// #2: Normal case
	result = tpl_display_query_config(this->backend->tpl_display,
			TPL_SURFACE_TYPE_WINDOW,
			8,
			8,
			8,
			8,
			24,
			NULL,
			NULL);

	ASSERT_EQ(TPL_ERROR_NONE, result);

	// #3: Abnormal case
	result = tpl_display_query_config(this->backend->tpl_display,
			TPL_SURFACE_TYPE_WINDOW,
			0,		// red size can't be zero
			8,
			8,
			8,
			24,
			NULL,
			NULL);

	ASSERT_NE(TPL_ERROR_NONE, result);
}


TEST_F(TPLDisplayTest, tpl_display_filter_config)
{
	tpl_result_t result;
	int test_visual_id = GBM_FORMAT_ARGB8888;
	result = tpl_display_filter_config(
			this->backend->tpl_display, &test_visual_id, 0);

	ASSERT_EQ(TPL_ERROR_NONE, result);
}


TEST_F(TPLDisplayTest, tpl_display_get_native_window_info)
{
	tpl_result_t result;
	int width, height;
	tbm_format format;

	result = tpl_display_get_native_window_info(this->backend->tpl_display,
			this->backend->surface_handle, &width, &height, &format,
			this->config.depth, 8);

	EXPECT_EQ(config.width, width);
	EXPECT_EQ(config.height, height);
	EXPECT_EQ(TBM_FORMAT_ARGB8888, format);

	ASSERT_EQ(TPL_ERROR_NONE, result);
}


// TODO: Need verification
TEST_F(TPLDisplayTestIfSupported,
	   tpl_display_query_supported_buffer_count_from_native_window)
{
	tpl_result_t result;
	int min, max;

	result = tpl_display_query_supported_buffer_count_from_native_window(
			this->backend->tpl_display, this->backend->surface_handle,
			&min, &max);

	// Backend may not support this function.
	EXPECT_EQ(TPL_ERROR_NONE, result);
}

