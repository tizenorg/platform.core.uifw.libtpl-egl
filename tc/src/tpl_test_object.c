#ifndef __TPL_OBJECT_TEST__
#define __TPL_OBJECT_TEST__

#include "tpl_test_util.h"


bool tpl_object_get_type_test(TPLNativeWnd* wnd )
{
	TPL_CHK_PARAM( !wnd );
	bool ret = true;
	LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------",__func__);

	//1.tpl_display_get
	wnd->tpl_display = NULL;
	wnd->tpl_display = tpl_display_get(TPL_BACKEND_WAYLAND, (tpl_handle_t)wnd->dpy);
	if(wnd->tpl_display == NULL)
	{
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
		ret = false;
		goto finish;
	}

	//2.tpl_surface_create
	wnd->tpl_surf = NULL;
	wnd->tpl_surf = tpl_surface_create(wnd->tpl_display, (tpl_handle_t)wnd->wnd, TPL_SURFACE_TYPE_WINDOW, TPL_FORMAT_ARGB8888);
	if(wnd->tpl_surf == NULL)
	{
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_create");
		ret = false;
		goto finish;
	}

	//3.begin frame
	tpl_surface_begin_frame(wnd->tpl_surf);

	//4.get buffer
	wnd->tpl_buf = NULL;
	wnd->tpl_buf = tpl_surface_get_buffer(wnd->tpl_surf, NULL);
	if (wnd->tpl_buf == NULL)
	{
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_get_buffer");
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

	//tpl_object_get_type:BUFFER
	obj_type = -1;
	obj_type = tpl_object_get_type((tpl_object_t *)wnd->tpl_buf);
	if (obj_type != TPL_OBJECT_BUFFER)
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


bool tpl_object_userdata_test(TPLNativeWnd* wnd )
{
	TPL_CHK_PARAM( !wnd );
	bool ret = true;
	LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------",__func__);

	//1.tpl_display_get
	wnd->tpl_display = NULL;
	wnd->tpl_display = tpl_display_get(TPL_BACKEND_WAYLAND, (tpl_handle_t)wnd->dpy);
	if(wnd->tpl_display == NULL)
	{
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
		ret = false;
		goto finish;
	}

	//2. set userdate
	tpl_object_set_user_data((tpl_object_t *)wnd->tpl_display,(void *)wnd->dpy,NULL);

	//3.get userdate
	void* get_dpy = NULL;
	get_dpy = (void *)tpl_object_get_user_data((tpl_object_t *)wnd->tpl_display);
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

bool tpl_object_reference_test(TPLNativeWnd* wnd )
{
	TPL_CHK_PARAM( !wnd );
	bool ret = true;
	LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------",__func__);

	//1.tpl_display_get
	wnd->tpl_display = NULL;
	wnd->tpl_display = tpl_display_get(TPL_BACKEND_WAYLAND, (tpl_handle_t)wnd->dpy);
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

bool tpl_object_abnormal_test(TPLNativeWnd* wnd )
{
	TPL_CHK_PARAM( !wnd );
	bool ret = true;
	LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------",__func__);


	//abnormal test
	tpl_object_get_type(NULL);
	tpl_object_set_user_data(NULL,NULL,NULL);
	tpl_object_get_user_data(NULL);
	tpl_object_reference(NULL);
	tpl_object_get_reference(NULL);
	tpl_object_unreference(NULL);


finish:
	if(true == ret)
		LOG("PASS", LOG_LEVEL_HIGH , "Pass:%s",__func__);
	else
		LOG("FAIL", LOG_LEVEL_HIGH , "Failed:%s",__func__);
	return ret;


}




#endif


