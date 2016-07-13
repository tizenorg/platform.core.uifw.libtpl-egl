/**
 * @file tpl-test-object.cpp
 * @brief TPL Test case of object 
 *
 */ 

#include "gtest/gtest.h"

#include "tpl-test.h"


class TPLObjectTest : public TPLTestBase {};


TEST_F(TPLObjectTest, tpl_object_get_type)
{
	int obj_type = -1;
	obj_type = tpl_object_get_type((tpl_object_t *)this->backend->tpl_display);
	ASSERT_EQ(TPL_OBJECT_DISPLAY, obj_type);

	obj_type = -1;
	obj_type = tpl_object_get_type((tpl_object_t *)this->backend->tpl_surface);
	ASSERT_EQ(TPL_OBJECT_SURFACE, obj_type);
}


TEST_F(TPLObjectTest, tpl_object_userdata_test)
{
	unsigned long key;

	// TODO: Check
	// set user data
	tpl_object_set_user_data((tpl_object_t *)this->backend->tpl_display, &key,
							 (void *)this->backend->display_handle, NULL);

	// get user data
	void* get_dpy = NULL;
	get_dpy = (void *)tpl_object_get_user_data(
			 (tpl_object_t *)this->backend->tpl_display, &key);

	ASSERT_NE((void *)NULL, get_dpy);
}


TEST_F(TPLObjectTest, tpl_object_reference)
{
	// tpl_object_reference
	tpl_object_reference((tpl_object_t *)this->backend->tpl_display);

	// tpl_object_get_reference
	int ref_count = -1;
	ref_count = tpl_object_get_reference(
			   (tpl_object_t *)this->backend->tpl_display);
	ASSERT_NE(-1, ref_count);

	// tpl_object_unreference
	int unref_count = -1;
	unref_count = tpl_object_unreference(
				 (tpl_object_t *)this->backend->tpl_display);
	ASSERT_EQ(ref_count - 1, unref_count);
}

