#ifndef __TPL_SURF_TEST__
#define __TPL_SURF_TEST__

#include "tpl_test_util.h"


bool tpl_surface_create_test(TPLNativeWnd* wnd)
{
    TPL_CHK_PARAM( !wnd );
    LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------",__func__);
    bool ret = true;
    tpl_display_t *tpl_display = NULL;
    //1.tpl_display_get
    tpl_display = tpl_display_get(TPL_BACKEND_WAYLAND, (tpl_handle_t)wnd->dpy);
    if(tpl_display == NULL)
    {
        LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
        ret = false;
        goto finish;
    }
    //2.tpl_surface_create
    tpl_surface_t *tpl_surf = NULL;
    tpl_surf = tpl_surface_create(tpl_display, (tpl_handle_t)wnd->wnd, TPL_SURFACE_TYPE_WINDOW, TPL_FORMAT_ARGB8888);
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

bool tpl_surface_get_arg_test(TPLNativeWnd* wnd)
{
    TPL_CHK_PARAM( !wnd );
    LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------",__func__);
    bool ret = true;
    tpl_display_t *tpl_display = NULL;
    //1.tpl_display_get
    tpl_display = tpl_display_get(TPL_BACKEND_WAYLAND, (tpl_handle_t)wnd->dpy);
    if(tpl_display == NULL)
    {
        LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
        ret = false;
        goto finish;
    }
    //2.tpl_surface_create
    tpl_surface_t *tpl_surf = NULL;
    tpl_surf = tpl_surface_create(tpl_display, (tpl_handle_t)wnd->wnd, TPL_SURFACE_TYPE_WINDOW, TPL_FORMAT_ARGB8888);
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

finish:
    if(true == ret)
        LOG("PASS", LOG_LEVEL_HIGH , "Pass:%s",__func__);
    else
        LOG("FAIL", LOG_LEVEL_HIGH , "Failed:%s",__func__);
    return ret;
}

bool tpl_surface_frame_test(TPLNativeWnd* wnd)
{
    TPL_CHK_PARAM( !wnd );
    LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------",__func__);
    bool ret = true;
    tpl_display_t *tpl_display = NULL;
    //1.tpl_display_get
    tpl_display = tpl_display_get(TPL_BACKEND_WAYLAND, (tpl_handle_t)wnd->dpy);
    if(tpl_display == NULL)
    {
        LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
        ret = false;
        goto finish;
    }
    //2.tpl_surface_create
    tpl_surface_t *tpl_surf = NULL;
    tpl_surf = tpl_surface_create(tpl_display, (tpl_handle_t)wnd->wnd, TPL_SURFACE_TYPE_WINDOW, TPL_FORMAT_ARGB8888);
    if(tpl_surf == NULL)
    {
        LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_create");
        ret = false;
        goto finish;
    }

    //3.begin frame
    tpl_surface_begin_frame(tpl_surf);

    //4.tpl_surface_validate_frame
    tpl_bool_t isvalid = tpl_surface_validate_frame(tpl_surf);

    //5. end frame
    tpl_surface_end_frame(tpl_surf);


finish:
    if(true == ret)
        LOG("PASS", LOG_LEVEL_HIGH , "Pass:%s",__func__);
    else
        LOG("FAIL", LOG_LEVEL_HIGH , "Failed:%s",__func__);

    return ret;
}


bool tpl_surface_get_buffer_test(TPLNativeWnd* wnd )
{
    TPL_CHK_PARAM( !wnd );
    bool ret = true;
    LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------",__func__);

    //1.tpl_display_get
    tpl_display_t *tpl_display = NULL;
    tpl_display = tpl_display_get(TPL_BACKEND_WAYLAND, (tpl_handle_t)wnd->dpy);
    if(tpl_display == NULL)
    {
        LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
        ret = false;
        goto finish;
    }

    //2.tpl_surface_create
    tpl_surface_t *tpl_surf = NULL;
    tpl_surf = tpl_surface_create(tpl_display, (tpl_handle_t)wnd->wnd, TPL_SURFACE_TYPE_WINDOW, TPL_FORMAT_ARGB8888);
    if(tpl_surf == NULL)
    {
        LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_create");
        ret = false;
        goto finish;
    }

    //3.begin frame
    tpl_surface_begin_frame(tpl_surf);

    //4.get buffer
    tpl_buffer_t *tpl_buf = NULL;
	tpl_buf = tpl_surface_get_buffer(tpl_surf, NULL);
	if (tpl_buf == NULL)
    {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_get_buffer");
        ret = false;
        goto finish;
	}

    //4.get buffer reset
    tpl_buffer_t *tpl_buf_r = NULL;
    tpl_bool_t reset;
	tpl_buf_r = tpl_surface_get_buffer(tpl_surf, &reset);
	if (tpl_buf_r == NULL)
    {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_get_buffer");
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




bool tpl_surface_post_test(TPLNativeWnd* wnd )
{
    TPL_CHK_PARAM( !wnd );
    bool ret = true;
    LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------",__func__);

    //1.tpl_display_get
    tpl_display_t *tpl_display = NULL;
    tpl_display = tpl_display_get(TPL_BACKEND_WAYLAND, (tpl_handle_t)wnd->dpy);
    if(tpl_display == NULL)
    {
        LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
        ret = false;
        goto finish;
    }

    //2.tpl_surface_create
    tpl_surface_t *tpl_surf = NULL;
    tpl_surf = tpl_surface_create(tpl_display, (tpl_handle_t)wnd->wnd, TPL_SURFACE_TYPE_WINDOW, TPL_FORMAT_ARGB8888);
    if(tpl_surf == NULL)
    {
        LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_create");
        ret = false;
        goto finish;
    }

    //3.begin frame
    tpl_surface_begin_frame(tpl_surf);

    //4.get buffer
    tpl_buffer_t *tpl_buf = NULL;
	tpl_buf = tpl_surface_get_buffer(tpl_surf, NULL);
	if (tpl_buf == NULL)
    {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_get_buffer");
        ret = false;
        goto finish;
	}

    //5.buffer map
    void *ptr = NULL;
    //int size = tpl_buf->width * tpl_buf->height * tpl_buf->depth;
    int size = wnd->width * wnd->height ;
    LOG("INFO", LOG_LEVEL_HIGH ,"width=%d,height=%d,size=%d\n",wnd->width , wnd->height , size);
    ptr = tpl_buffer_map(tpl_buf, size);
    if (ptr == NULL)
    {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_buffer_map");
        ret = false;
        goto finish;
	}

    //6.write
    int *p = NULL;int j=0;
    if(ptr!=NULL)
    {
        memset(ptr,size,0xFFFF);
    }
    for(j = 0; j <size;j++)
    {
        p = (int*) ptr;
        *(p+j) = 0xFF00;
    }
    LOG("INFO", LOG_LEVEL_LOW , "succ:write completed!");

    //7.end frame
    tpl_surface_end_frame(tpl_surf);

    //8.set post interval
    int interval_set = 2;
    tpl_surface_set_post_interval(tpl_surf, interval_set);

    //9. get post interval;
    int interval_get = tpl_surface_get_post_interval(tpl_surf);

    if(interval_get!=interval_set)
    {
         LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_get_post_interval");
         ret = false;
         goto finish;
    }

    //10.post
    //for(k=0;k<1000;k++)
    {
        tpl_surface_post(tpl_surf);
    }
    LOG("INFO", LOG_LEVEL_LOW , "After posted!!!");

    int k=0;
    while(k<20)
	{
		usleep(1000*1000);
		LOG("INFO", LOG_LEVEL_LOW , "sleep %d ...",k++);
	}

finish:
    if(true == ret)
        LOG("PASS", LOG_LEVEL_HIGH , "Pass:%s",__func__);
    else
        LOG("FAIL", LOG_LEVEL_HIGH , "Failed:%s",__func__);
    return ret;


}


bool tpl_surface_abnormal_test(TPLNativeWnd* wnd)
{
    TPL_CHK_PARAM( !wnd );
    LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------",__func__);
    bool ret = true;
    tpl_surface_t *tpl_surf = NULL;

    //1.tpl_display_get
    tpl_display_t *tpl_display = NULL;
    tpl_display = tpl_display_get(TPL_BACKEND_WAYLAND, (tpl_handle_t)wnd->dpy);
    if(tpl_display == NULL)
    {
        LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
        ret = false;
        goto finish;
    }

    //abnormal test
    tpl_surf = NULL;
    tpl_surf = tpl_surface_create(NULL, NULL, TPL_SURFACE_TYPE_WINDOW, TPL_FORMAT_ARGB8888);
    tpl_surf = tpl_surface_create(tpl_display, (tpl_handle_t)wnd->wnd, 10, TPL_FORMAT_ARGB8888);
    tpl_surf = tpl_surface_create(tpl_display, (tpl_handle_t)wnd->wnd, TPL_SURFACE_TYPE_WINDOW, TPL_FORMAT_INVALID);
/*    if(tpl_surf != NULL)
    {
        LOG("ERRO", LOG_LEVEL_HIGH , "abnormal test failed:%s",__func__);
        ret = false;
        goto finish;
    }
*/
    //abnormal test
    int width = 0, height = 0;
    tpl_surface_get_display(NULL);
    tpl_surface_get_native_handle(NULL);
    tpl_surface_get_type(NULL);
    tpl_surface_get_size(NULL,&width,&height);
    tpl_surface_get_buffer(NULL,NULL);
    tpl_surface_set_post_interval(NULL, 2);
    tpl_surface_get_post_interval(NULL);
    tpl_surface_post(NULL);
    tpl_surface_begin_frame(NULL);
    tpl_surface_validate_frame(NULL);
    tpl_surface_end_frame(NULL);

finish:
    if(true == ret)
        LOG("PASS", LOG_LEVEL_HIGH , "Pass:%s",__func__);
    else
        LOG("FAIL", LOG_LEVEL_HIGH , "Failed:%s",__func__);
    return ret;
}









#endif


