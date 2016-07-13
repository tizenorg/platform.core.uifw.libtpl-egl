/**
 * @file tpl-test-display.cpp
 * @brief TPL Test case of display 
 *
 */ 

#include "gtest/gtest.h"

#include "tpl-test.h"


class TPLDisplayTest : public TPLTestBase {};


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

