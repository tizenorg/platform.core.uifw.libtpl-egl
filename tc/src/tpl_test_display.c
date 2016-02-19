#ifndef __TPL_DISPLAY_TEST__
#define __TPL_DISPLAY_TEST__

#include "tpl_test_util.h"


bool tpl_display_get_test (TPLNativeWnd *wnd)
{
	TPL_CHK_PARAM( !wnd );
	LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------", __func__);
	bool ret = true;
	wnd->tpl_display = NULL;
	wnd->tpl_display = tpl_display_get(TPL_BACKEND_WAYLAND, (tpl_handle_t)wnd->dpy);
	if (wnd->tpl_display == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
		ret = false;
		goto finish;
	}
finish:
	if (true == ret)
		LOG("PASS", LOG_LEVEL_HIGH , "Pass:%s", __func__);
	else
		LOG("FAIL", LOG_LEVEL_HIGH , "Failed:%s", __func__);
	return ret;
}


bool tpl_display_bind_client_display_test(TPLNativeWnd *wnd)
{
	TPL_CHK_PARAM( !wnd );
	LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s---", __func__);
	bool ret = true;
	wnd->tpl_display = NULL;
	wnd->tpl_display = tpl_display_get(TPL_BACKEND_WAYLAND, (tpl_handle_t)wnd->dpy);
	if (wnd->tpl_display == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
		ret = false;
		goto finish;
	}

	// bind display handle
	tpl_bool_t result = false;
	result = tpl_display_bind_client_display_handle(wnd->tpl_display,
			(tpl_handle_t)wnd->dpy);
	if (result == false) {
		LOG("ERRO", LOG_LEVEL_HIGH , " failed:tpl_display_bind_client_display_handle");
		ret = false;
		goto finish;
	}

	// unbind display handle
	result = false;
	result = tpl_display_unbind_client_display_handle(wnd->tpl_display,
			(tpl_handle_t)wnd->dpy);
	if (result == false) {
		LOG("ERRO", LOG_LEVEL_HIGH ,
		    " failed:tpl_display_unbind_client_display_handle");
		ret = false;
		goto finish;
	}

finish:
	if (true == ret)
		LOG("PASS", LOG_LEVEL_HIGH , "Pass:%s", __func__);
	else
		LOG("FAIL", LOG_LEVEL_HIGH , "Failed:%s", __func__);
	return ret;
}


bool tpl_display_get_arg_test (TPLNativeWnd *wnd)
{
	TPL_CHK_PARAM( !wnd );
	LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------", __func__);
	bool ret = true;
	wnd->tpl_display = NULL;
	wnd->tpl_display = tpl_display_get(TPL_BACKEND_WAYLAND, (tpl_handle_t)wnd->dpy);
	if (wnd->tpl_display == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
		ret = false;
		goto finish;
	}

	//tpl_display_get_backend_type
	tpl_backend_type_t backend_type = tpl_display_get_backend_type(
			wnd->tpl_display);
	if (backend_type != TPL_BACKEND_WAYLAND) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get_backend_type");
		ret = false;
		goto finish;
	}

	//tpl_display_get_native_handle
	tpl_handle_t test_handle = NULL;
	test_handle = tpl_display_get_native_handle(wnd->tpl_display);
	if (test_handle == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get_native_handle");
		ret = false;
		goto finish;
	}

finish:
	if (true == ret)
		LOG("PASS", LOG_LEVEL_HIGH , "Pass:%s", __func__);
	else
		LOG("FAIL", LOG_LEVEL_HIGH , "Failed:%s", __func__);
	return ret;
}


bool tpl_display_query_config_test (TPLNativeWnd *wnd)
{
	TPL_CHK_PARAM( !wnd );
	LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------", __func__);
	bool ret = true;
	wnd->tpl_display = NULL;
	wnd->tpl_display = tpl_display_get(TPL_BACKEND_WAYLAND, (tpl_handle_t)wnd->dpy);
	if (wnd->tpl_display == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
		ret = false;
		goto finish;
	}
	//query config
	tpl_bool_t result = false;
	result = tpl_display_query_config(wnd->tpl_display,
					  TPL_SURFACE_TYPE_WINDOW,
					  8,
					  8,
					  8,
					  8,
					  32,
					  NULL,
					  NULL);
	if (result == false ) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_query_config");
		ret = false;
		goto finish;
	}

	result = tpl_display_query_config(wnd->tpl_display,
					  TPL_SURFACE_TYPE_WINDOW,
					  8,
					  8,
					  8,
					  8,
					  24,
					  NULL,
					  NULL);
	if (result == false ) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_query_config");
		ret = false;
		goto finish;
	}

	result = tpl_display_query_config(wnd->tpl_display,
					  TPL_SURFACE_TYPE_WINDOW,
					  0,
					  8,
					  8,
					  8,
					  24,
					  NULL,
					  NULL);
	if (result != false ) { //unmatched case
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_query_config");
		ret = false;
		goto finish;
	}

finish:
	if (true == ret)
		LOG("PASS", LOG_LEVEL_HIGH , "Pass:%s", __func__);
	else
		LOG("FAIL", LOG_LEVEL_HIGH , "Failed:%s", __func__);
	return ret;
}

bool tpl_display_filter_config_test (TPLNativeWnd *wnd)
{
	TPL_CHK_PARAM( !wnd );
	LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------", __func__);
	bool ret = true;
	wnd->tpl_display = NULL;
	wnd->tpl_display = tpl_display_get(TPL_BACKEND_WAYLAND, (tpl_handle_t)wnd->dpy);
	if (wnd->tpl_display == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
		ret = false;
		goto finish;
	}

	//filt config
	tpl_bool_t result = false;
	int test_visual_id = GBM_FORMAT_ARGB8888;
	result = tpl_display_filter_config(wnd->tpl_display, &test_visual_id, 0);
	if (result == false ) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_filter_config");
		ret = false;
		goto finish;
	}

	//filt config, unmatched case
	result = tpl_display_filter_config(wnd->tpl_display, &test_visual_id, 8);
	if (result != false ) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_filter_config");
		ret = false;
		goto finish;
	}
	test_visual_id = GBM_FORMAT_XRGB8888;
	result = tpl_display_filter_config(wnd->tpl_display, &test_visual_id, 0);
	if (result != false ) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_filter_config");
		ret = false;
		goto finish;
	}


finish:
	if (true == ret)
		LOG("PASS", LOG_LEVEL_HIGH , "Pass:%s", __func__);
	else
		LOG("FAIL", LOG_LEVEL_HIGH , "Failed:%s", __func__);
	return ret;
}


bool tpl_display_abnormal_test (TPLNativeWnd *wnd)
{
	TPL_CHK_PARAM( !wnd );
	LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------", __func__);
	bool ret = true;
	wnd->tpl_display = NULL;
	wnd->tpl_display = tpl_display_get(TPL_BACKEND_WAYLAND, (tpl_handle_t)wnd->dpy);
	if (wnd->tpl_display == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
		ret = false;
		goto finish;
	}

	//abnormal test
	wnd->tpl_display = tpl_display_get(TPL_BACKEND_WAYLAND, NULL);
	if (wnd->tpl_display != NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "abnormal test failed:tpl_display_get");
		ret = false;
		goto finish;
	}


	//abnormal test
	tpl_display_bind_client_display_handle(NULL, NULL);
	tpl_display_unbind_client_display_handle(NULL, NULL);
	tpl_display_get_backend_type(NULL);
	tpl_display_get_native_handle(NULL);
	tpl_display_filter_config(NULL, NULL, 0);
	tpl_display_query_config(NULL, TPL_SURFACE_TYPE_PIXMAP, 0, 8, 8, 8, 24, NULL,
				 NULL);


finish:
	if (true == ret)
		LOG("PASS", LOG_LEVEL_HIGH , "Pass:%s", __func__);
	else
		LOG("FAIL", LOG_LEVEL_HIGH , "Failed:%s", __func__);
	return ret;
}


#endif

