/**
 * @file tpl-test-display.cpp
 * @brief TPL Test case of display 
 *
 */ 

#include "tpl-test.h"

#include "gtest/gtest.h"


TEST_F(TPLTest, tpl_display_get)
{
	ASSERT_NE((void *)NULL, tpl_display_get((tpl_handle_t)wl_disp.display));
}

TEST_F(TPLTest, tpl_display_get_native_handle)
{
	ASSERT_NE((void *)NULL, tpl_display_get_native_handle(tpl_display));
}

TEST_F(TPLTest, tpl_display_query_config)
{
	tpl_result_t result;
	
	// #1
	result = tpl_display_query_config(tpl_display,
			TPL_SURFACE_TYPE_WINDOW,
			8,
			8,
			8,
			8,
			32,
			NULL,
			NULL);
	
	ASSERT_EQ(TPL_ERROR_NONE, result);
   
	// #2
	result = tpl_display_query_config(tpl_display,
			TPL_SURFACE_TYPE_WINDOW,
			8,
			8,
			8,
			8,
			24,
			NULL,
			NULL);

	ASSERT_EQ(TPL_ERROR_NONE, result);
	
	// #3 : Abnormal Case
	result = tpl_display_query_config(tpl_display,
			TPL_SURFACE_TYPE_WINDOW,
			0,
			8,
			8,
			8,
			24,
			NULL,
			NULL);

	ASSERT_NE(TPL_ERROR_NONE, result);
}

TEST_F(TPLTest, tpl_display_filter_config)
{
	tpl_result_t result;
	int test_visual_id = GBM_FORMAT_ARGB8888;
	result = tpl_display_filter_config(tpl_display, &test_visual_id, 0);
	
	ASSERT_EQ(TPL_ERROR_NONE, result);
}
