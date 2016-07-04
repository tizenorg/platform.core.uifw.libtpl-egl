#ifndef __TPL_OBJECT_TEST__
#define __TPL_OBJECT_TEST__

#include "tpl_test_util.h"


bool
tpl_object_get_type_test(TPLNativeWnd* wnd)
{
	TPL_CHK_PARAM(!wnd);
	bool ret = true;
	LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------",__func__);

	//1.tpl_display_get
	wnd->tpl_display = NULL;
	wnd->tpl_display = tpl_display_get((tpl_handle_t)wnd->dpy);
	if(wnd->tpl_display == NULL)
	{
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
		ret = false;
		goto finish;
	}

	//2.tpl_surface_create
	wnd->tpl_surf = NULL;
	wnd->tpl_surf = tpl_surface_create(wnd->tpl_display, (tpl_handle_t)wnd->wnd,
				   TPL_SURFACE_TYPE_WINDOW, TBM_FORMAT_ARGB8888);
	if(wnd->tpl_surf == NULL)
	{
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_create");
		ret = false;
		goto finish;
	}

	//tpl_object_get_type:DISPLAY
	int obj_type = -1;
	obj_type = tpl_object_get_type((tpl_object_t *)wnd->tpl_display);
	if (obj_type != TPL_OBJECT_DISPLAY)
	{
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_object_get_type");
		ret = false;
		goto finish;
	}

	//tpl_object_get_type:SURFACE
	obj_type = -1;
	obj_type = tpl_object_get_type((tpl_object_t *)wnd->tpl_surf);
	if (obj_type != TPL_OBJECT_SURFACE)
	{
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_object_get_type");
		ret = false;
		goto finish;
	}


finish:
	if(true == ret)
		LOG("PASS", LOG_LEVEL_HIGH , "Pass:%s",__func__);
	else
		LOG("FAIL", LOG_LEVEL_HIGH , "Failed:%s",__func__);
	return ret;

}


bool
tpl_object_userdata_test(TPLNativeWnd* wnd)
{
	TPL_CHK_PARAM(!wnd);
	bool ret = true;
	LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------",__func__);

	//1.tpl_display_get
	wnd->tpl_display = NULL;
	wnd->tpl_display = tpl_display_get((tpl_handle_t)wnd->dpy);
	if(wnd->tpl_display == NULL)
	{
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
		ret = false;
		goto finish;
	}

    unsigned long key;

	//2. set user data
	tpl_object_set_user_data((tpl_object_t *)wnd->tpl_display, &key,
							 (void *)wnd->dpy,NULL);

	//3.get user data
	void* get_dpy = NULL;
	get_dpy = (void *)tpl_object_get_user_data(
			 (tpl_object_t *)wnd->tpl_display, &key);
	if(get_dpy == NULL)
	{
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_object_get_user_data");
		ret = false;
		goto finish;
	}


finish:
	if(true == ret)
		LOG("PASS", LOG_LEVEL_HIGH , "Pass:%s",__func__);
	else
		LOG("FAIL", LOG_LEVEL_HIGH , "Failed:%s",__func__);
	return ret;


}

bool
tpl_object_reference_test(TPLNativeWnd* wnd)
{
	TPL_CHK_PARAM(!wnd);
	bool ret = true;
	LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------",__func__);

	//1.tpl_display_get
	wnd->tpl_display = NULL;
	wnd->tpl_display = tpl_display_get((tpl_handle_t)wnd->dpy);
	if(wnd->tpl_display == NULL)
	{
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
		ret = false;
		goto finish;
	}

	//2. tpl_object_reference
	tpl_object_reference((tpl_object_t *)wnd->tpl_display);

	//3.tpl_object_get_reference
	int ref_count = -1;
	ref_count = tpl_object_get_reference((tpl_object_t *)wnd->tpl_display);
	if(ref_count == -1)
	{
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_object_get_reference");
		ret = false;
		goto finish;
	}

	//4.tpl_object_unreference
	int unref_count = -1;
	unref_count = tpl_object_unreference((tpl_object_t *)wnd->tpl_display);
	if(unref_count != ref_count-1)
	{
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_object_unreference");
		ret = false;
		goto finish;
	}

finish:
	if(true == ret)
		LOG("PASS", LOG_LEVEL_HIGH , "Pass:%s",__func__);
	else
		LOG("FAIL", LOG_LEVEL_HIGH , "Failed:%s",__func__);
	return ret;
}


#endif

