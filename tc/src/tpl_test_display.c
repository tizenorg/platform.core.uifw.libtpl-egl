#ifndef __TPL_DISPLAY_TEST__
#define __TPL_DISPLAY_TEST__

#include "tpl_test_util.h"


bool tpl_display_create_test (TPLNativeWnd *wnd)
{
    TPL_CHK_PARAM( !wnd );
    LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------", __func__);
    bool ret = true;
    wnd->tpl_display = NULL;
    wnd->tpl_display = tpl_display_create(TPL_BACKEND_WAYLAND, (tpl_handle_t)wnd->dpy);
    if (wnd->tpl_display == NULL) {
        LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_create");
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

bool tpl_display_get_test (TPLNativeWnd *wnd)
{
	TPL_CHK_PARAM( !wnd );
	LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------", __func__);
	bool ret = true;
	wnd->tpl_display = NULL;
	wnd->tpl_display = tpl_display_get((tpl_handle_t)wnd->dpy);
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

bool tpl_display_get_native_handle_test (TPLNativeWnd *wnd)
{
    TPL_CHK_PARAM( !wnd );
    LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------", __func__);
    bool ret = true;
    wnd->tpl_display = NULL;
    wnd->tpl_display = tpl_display_get((tpl_handle_t)wnd->dpy);
    if (wnd->tpl_display == NULL) {
        LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
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
	wnd->tpl_display = tpl_display_get((tpl_handle_t)wnd->dpy);
	if (wnd->tpl_display == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
		ret = false;
		goto finish;
	}

	//query config
	tpl_result_t result = TPL_ERROR_NONE;
	result = tpl_display_query_config(wnd->tpl_display,
					  TPL_SURFACE_TYPE_WINDOW,
					  8,
					  8,
					  8,
					  8,
					  32,
					  NULL,
					  NULL);
	if (result != TPL_ERROR_NONE ) {
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
	if (result != TPL_ERROR_NONE ) {
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
	if (result == TPL_ERROR_NONE ) { //unmatched case
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
	wnd->tpl_display = tpl_display_get((tpl_handle_t)wnd->dpy);
	if (wnd->tpl_display == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
		ret = false;
		goto finish;
	}

	// Filt config
	tpl_result_t result = false;
	int test_visual_id = GBM_FORMAT_ARGB8888;
	result = tpl_display_filter_config(wnd->tpl_display, &test_visual_id, 0);
	if (result != TPL_ERROR_NONE ) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_filter_config");
		ret = false;
		goto finish;
	}

    // No test for unmatched case
    // Current implementation ignore all arguments other than tpl_display

finish:
	if (true == ret)
		LOG("PASS", LOG_LEVEL_HIGH , "Pass:%s", __func__);
	else
		LOG("FAIL", LOG_LEVEL_HIGH , "Failed:%s", __func__);
	return ret;
}


#endif

