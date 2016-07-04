#ifndef __TPL_SURF_TEST__
#define __TPL_SURF_TEST__

#include "tpl_test_util.h"


bool tpl_surface_create_test(TPLNativeWnd* wnd)
{
	TPL_CHK_PARAM( !wnd );
	LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------",__func__);
	bool ret = true;
	wnd->tpl_display = NULL;
	//1.tpl_display_get
	wnd->tpl_display = tpl_display_get((tpl_handle_t)wnd->dpy);
	if(wnd->tpl_display == NULL)
	{
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
		ret = false;
		goto finish;
	}
	//2.tpl_surface_create
	tpl_surface_t *tpl_surf = NULL;
	tpl_surf = tpl_surface_create(wnd->tpl_display, (tpl_handle_t)wnd->wnd, TPL_SURFACE_TYPE_WINDOW, TBM_FORMAT_ARGB8888);
	if(tpl_surf == NULL)
	{
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_create");
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

bool tpl_surface_validate_test(TPLNativeWnd* wnd)
{
	TPL_CHK_PARAM( !wnd );
	LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------",__func__);
	bool ret = true;

    tpl_bool_t result = tpl_surface_validate(wnd->tpl_surf);
    if (result == TPL_FALSE)
    {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_validate");
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

bool tpl_surface_get_arg_test(TPLNativeWnd* wnd)
{
	TPL_CHK_PARAM( !wnd );
	LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------",__func__);
	bool ret = true;
	wnd->tpl_display = NULL;
	//1.tpl_display_get
	wnd->tpl_display = tpl_display_get((tpl_handle_t)wnd->dpy);
	if(wnd->tpl_display == NULL)
	{
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
		ret = false;
		goto finish;
	}
	//2.tpl_surface_create
	tpl_surface_t *tpl_surf = NULL;
	tpl_surf = tpl_surface_create(wnd->tpl_display, (tpl_handle_t)wnd->wnd, TPL_SURFACE_TYPE_WINDOW, TBM_FORMAT_ARGB8888);
	if(tpl_surf == NULL)
	{
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_create");
		ret = false;
		goto finish;
	}

	//tpl_surface_get_display test
	tpl_display_t* test_dpy = tpl_surface_get_display(tpl_surf);
	if(test_dpy == NULL)
	{
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_get_display");
		ret = false;
		goto finish;
	}
	//tpl_surface_get_native_handle
	tpl_handle_t test_handle = tpl_surface_get_native_handle(tpl_surf);
	if(test_handle == NULL)
	{
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_get_native_handle");
		ret = false;
		goto finish;
	}
	//tpl_surface_get_type test
	tpl_surface_type_t test_type = tpl_surface_get_type(tpl_surf);
	if(test_type != TPL_SURFACE_TYPE_WINDOW)
	{
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_get_type");
		ret = false;
		goto finish;
	}

	//tpl_surface_get_size test
    int width, height;
    tpl_result_t test_result = tpl_surface_get_size(tpl_surf, &width, &height);
	//if(test_result != TPL_ERROR_NONE || (width != tpl_surf->width || height != tpl_surf->height))
	if(test_result != TPL_ERROR_NONE)
	{
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_get_size");
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

bool tpl_surface_dequeue_and_enqueue_buffer_test(TPLNativeWnd* wnd)
{
	TPL_CHK_PARAM( !wnd );
	LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------",__func__);
	bool ret = true;

    // tpl_surface_dequeue_buffer	
    tbm_surface_h tbm_surf = NULL;
    tbm_surf = tpl_surface_dequeue_buffer(wnd->tpl_surf);
	if(tbm_surf == NULL)
	{
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_dequeue_buffer");
		ret = false;
		goto finish;
	}

    // get and set post interval
    int interval_set = 2;
    tpl_surface_set_post_interval(wnd->tpl_surf, interval_set);
    int interval_get = tpl_surface_get_post_interval(wnd->tpl_surf);
    if(interval_get != interval_set)
    {
         LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_get_post_interval");
         ret = false;
         goto finish;
    }

    // tpl_surface_enqueue_buffer
    wnd->tbm_surf = tbm_surf;
    tpl_result_t result = TPL_ERROR_INVALID_PARAMETER; 
	result = tpl_surface_enqueue_buffer(wnd->tpl_surf, wnd->tbm_surf);
    if(result != TPL_ERROR_NONE)
	{
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_enqueue_buffer");
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

