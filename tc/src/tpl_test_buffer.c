#ifndef __TPL_BUF_TEST__
#define __TPL_BUF_TEST__

#include "tpl_test_util.h"


bool tpl_buffer_map_unmap_test(TPLNativeWnd *wnd )
{
	TPL_CHK_PARAM( !wnd );
	bool ret = true;
	LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------", __func__);

	//1.tpl_display_get
	wnd->tpl_display = NULL;
	wnd->tpl_display = tpl_display_get(TPL_BACKEND_WAYLAND, (tpl_handle_t)wnd->dpy);
	if (wnd->tpl_display == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
		ret = false;
		goto finish;
	}

	//2.tpl_surface_create
	wnd->tpl_surf = NULL;
	wnd->tpl_surf = tpl_surface_create(wnd->tpl_display , (tpl_handle_t)wnd->wnd,
					   TPL_SURFACE_TYPE_WINDOW, TPL_FORMAT_ARGB8888);
	if (wnd->tpl_surf == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_create");
		ret = false;
		goto finish;
	}

	//3.begin frame
	tpl_surface_begin_frame(wnd->tpl_surf);

	//4.get buffer
	wnd->tpl_buf = NULL;
	wnd->tpl_buf = tpl_surface_get_buffer(wnd->tpl_surf, NULL);
	if (wnd->tpl_buf == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_get_buffer");
		ret = false;
		goto finish;
	}

	//5.buffer map
	void *ptr = NULL;
	//int size = tpl_buf->width * tpl_buf->height * tpl_buf->depth;
	int size = wnd->width * wnd->height ;
	LOG("INFO", LOG_LEVEL_LOW , "width=%d,height=%d,size=%d\n", wnd->width ,
	    wnd->height , size);
	ptr = tpl_buffer_map(wnd->tpl_buf, size);
	if (ptr == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_buffer_map");
		ret = false;
		goto finish;
	}

	//5.buffer unmap
	tpl_buffer_unmap(wnd->tpl_buf, ptr, size);


finish:
	if (true == ret)
		LOG("PASS", LOG_LEVEL_HIGH , "Pass:%s", __func__);
	else
		LOG("FAIL", LOG_LEVEL_HIGH , "Failed:%s", __func__);
	return ret;


}

bool tpl_buffer_lock_unlock_test(TPLNativeWnd *wnd )
{
	TPL_CHK_PARAM( !wnd );
	bool ret = true;
	LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------", __func__);

	//1.tpl_display_get
	wnd->tpl_display = NULL;
	wnd->tpl_display = tpl_display_get(TPL_BACKEND_WAYLAND, (tpl_handle_t)wnd->dpy);
	if (wnd->tpl_display == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
		ret = false;
		goto finish;
	}

	//2.tpl_surface_create
	wnd->tpl_surf = NULL;
	wnd->tpl_surf = tpl_surface_create(wnd->tpl_display, (tpl_handle_t)wnd->wnd,
					   TPL_SURFACE_TYPE_WINDOW, TPL_FORMAT_ARGB8888);
	if (wnd->tpl_surf == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_create");
		ret = false;
		goto finish;
	}

	//3.begin frame
	tpl_surface_begin_frame(wnd->tpl_surf);

	//4.get buffer
	wnd->tpl_buf = NULL;
	wnd->tpl_buf = tpl_surface_get_buffer(wnd->tpl_surf, NULL);
	if (wnd->tpl_buf == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_get_buffer");
		ret = false;
		goto finish;
	}

	//5.buffer lock
	tpl_bool_t result = false;
	result = tpl_buffer_lock(wnd->tpl_buf, TPL_LOCK_USAGE_GPU_READ);
	if (result == true) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_buffer_lock");
		ret = false;
		goto finish;
	}
	tpl_buffer_unlock(wnd->tpl_buf);

	//TPL_LOCK_USAGE_GPU_WRITE
	result = tpl_buffer_lock(wnd->tpl_buf, TPL_LOCK_USAGE_GPU_WRITE);
	if (result == true) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_buffer_lock");
		ret = false;
		goto finish;
	}
	tpl_buffer_unlock(wnd->tpl_buf);

	//TPL_LOCK_USAGE_CPU_READ
	result = tpl_buffer_lock(wnd->tpl_buf, TPL_LOCK_USAGE_CPU_READ);
	if (result == true) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_buffer_lock");
		ret = false;
		goto finish;
	}
	tpl_buffer_unlock(wnd->tpl_buf);

	//TPL_LOCK_USAGE_CPU_WRITE
	result = tpl_buffer_lock(wnd->tpl_buf, TPL_LOCK_USAGE_CPU_WRITE);
	if (result == true) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_buffer_lock");
		ret = false;
		goto finish;
	}
	tpl_buffer_unlock(wnd->tpl_buf);


finish:
	if (true == ret)
		LOG("PASS", LOG_LEVEL_HIGH , "Pass:%s", __func__);
	else
		LOG("FAIL", LOG_LEVEL_HIGH , "Failed:%s", __func__);
	return ret;


}


bool tpl_buffer_get_arg_test(TPLNativeWnd *wnd )
{
	TPL_CHK_PARAM( !wnd );
	bool ret = true;
	LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------", __func__);

	//1.tpl_display_get
	wnd->tpl_display = NULL;
	wnd->tpl_display = tpl_display_get(TPL_BACKEND_WAYLAND, (tpl_handle_t)wnd->dpy);
	if (wnd->tpl_display == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
		ret = false;
		goto finish;
	}

	//2.tpl_surface_create
	wnd->tpl_surf = NULL;
	wnd->tpl_surf = tpl_surface_create(wnd->tpl_display, (tpl_handle_t)wnd->wnd,
					   TPL_SURFACE_TYPE_WINDOW, TPL_FORMAT_ARGB8888);
	if (wnd->tpl_surf == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_create");
		ret = false;
		goto finish;
	}

	//3.begin frame
	tpl_surface_begin_frame(wnd->tpl_surf);

	//4.get buffer
	wnd->tpl_buf = NULL;
	wnd->tpl_buf = tpl_surface_get_buffer(wnd->tpl_surf, NULL);
	if (wnd->tpl_buf == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_get_buffer");
		ret = false;
		goto finish;
	}

	//tpl_buffer_get_key
	unsigned int key = 0;
	key = tpl_buffer_get_key(wnd->tpl_buf);
	if (key == 0) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_buffer_get_key");
		ret = false;
		goto finish;
	}

	//tpl_buffer_get_fd
	int fd = -1;
	fd = tpl_buffer_get_fd(wnd->tpl_buf);
	if (fd == -1) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_buffer_get_fd");
		ret = false;
		goto finish;
	}

	//tpl_buffer_get_age
	int age = -1;
	age = tpl_buffer_get_age(wnd->tpl_buf);
	if (age == 0) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_buffer_get_age");
		ret = false;
		goto finish;
	}

	//tpl_buffer_get_surface
	tpl_surface_t *test_surf = NULL;
	test_surf = tpl_buffer_get_surface(wnd->tpl_buf);
	if (test_surf == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_buffer_get_surface");
		ret = false;
		goto finish;
	}

	//tpl_buffer_get_size
	int width = 0, heigh = 0;
	tpl_buffer_get_size(wnd->tpl_buf, &width, &heigh);
	if ((width == 0) || (heigh == 0)) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_buffer_get_size");
		ret = false;
		goto finish;
	}

	//tpl_buffer_get_depth
	int depth = 0;
	depth = tpl_buffer_get_depth(wnd->tpl_buf);
	if (depth == 0) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_buffer_get_depth");
		ret = false;
		goto finish;
	}
	//tpl_buffer_get_pitch
	int pitch = 0;
	pitch = tpl_buffer_get_fd(wnd->tpl_buf);
	if (pitch == 0) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_buffer_get_fd");
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


bool tpl_buffer_create_native_buffer_test(TPLNativeWnd *wnd )
{
	TPL_CHK_PARAM( !wnd );
	bool ret = true;
	LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s---", __func__);

	//1.tpl_display_get
	wnd->tpl_display = NULL;
	wnd->tpl_display = tpl_display_get(TPL_BACKEND_WAYLAND, (tpl_handle_t)wnd->dpy);
	if (wnd->tpl_display == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
		ret = false;
		goto finish;
	}

	//2.tpl_surface_create
	wnd->tpl_surf = NULL;
	wnd->tpl_surf = tpl_surface_create(wnd->tpl_display, (tpl_handle_t)wnd->wnd,
					   TPL_SURFACE_TYPE_WINDOW, TPL_FORMAT_ARGB8888);
	if (wnd->tpl_surf == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_create");
		ret = false;
		goto finish;
	}

	//3.begin frame
	tpl_surface_begin_frame(wnd->tpl_surf);

	//4.get buffer
	wnd->tpl_buf = NULL;
	wnd->tpl_buf = tpl_surface_get_buffer(wnd->tpl_surf, NULL);
	if (wnd->tpl_buf == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_get_buffer");
		ret = false;
		goto finish;
	}

	//create native buffer
	void *native_buffer = NULL;
	native_buffer = tpl_buffer_create_native_buffer(wnd->tpl_buf);
	if (native_buffer == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_buffer_create_native_buffer");
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


bool tpl_buffer_abnormal_test(TPLNativeWnd *wnd )
{
	TPL_CHK_PARAM( !wnd );
	bool ret = true;
	LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------", __func__);

	//1.tpl_display_get
	wnd->tpl_display = NULL;
	wnd->tpl_display = tpl_display_get(TPL_BACKEND_WAYLAND, (tpl_handle_t)wnd->dpy);
	if (wnd->tpl_display == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
		ret = false;
		goto finish;
	}

	//2.tpl_surface_create
	wnd->tpl_surf = NULL;
	wnd->tpl_surf = tpl_surface_create(wnd->tpl_display, (tpl_handle_t)wnd->wnd,
					   TPL_SURFACE_TYPE_WINDOW, TPL_FORMAT_ARGB8888);
	if (wnd->tpl_surf == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_create");
		ret = false;
		goto finish;
	}

	//3.begin frame
	tpl_surface_begin_frame(wnd->tpl_surf);

	//4.get buffer
	wnd->tpl_buf = NULL;
	wnd->tpl_buf = tpl_surface_get_buffer(wnd->tpl_surf, NULL);
	if (wnd->tpl_buf == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_get_buffer");
		ret = false;
		goto finish;
	}

	//abnormal test

	tpl_buffer_lock(wnd->tpl_buf, TPL_LOCK_USAGE_INVALID);
	int test_width = 0, test_heigh = 0;
	tpl_buffer_map(NULL, 0);
	tpl_buffer_unmap(NULL, NULL, 0);
	tpl_buffer_lock(NULL, TPL_LOCK_USAGE_INVALID);
	tpl_buffer_unlock(NULL);
	tpl_buffer_get_key(NULL);
	tpl_buffer_get_fd(NULL);
	tpl_buffer_get_age(NULL);
	tpl_buffer_get_surface(NULL);
	tpl_buffer_get_size(NULL, &test_width, &test_heigh);
	tpl_buffer_get_depth(NULL);
	tpl_buffer_get_pitch(NULL);
	tpl_buffer_create_native_buffer(NULL);

finish:
	if (true == ret)
		LOG("PASS", LOG_LEVEL_HIGH , "Pass:%s", __func__);
	else
		LOG("FAIL", LOG_LEVEL_HIGH , "Failed:%s", __func__);
	return ret;


}


bool tpl_buffer_stress_test(TPLNativeWnd *wnd )
{
	TPL_CHK_PARAM( !wnd );
	bool ret = true;
	int index = 0;
	int size = 0;
	LOG("INFO", LOG_LEVEL_LOW , "-------begin:%s-------", __func__);

	//1.tpl_display _get
	wnd->tpl_display = NULL;
	wnd->tpl_display = tpl_display_get(TPL_BACKEND_WAYLAND, (tpl_handle_t)wnd->dpy);
	if (wnd->tpl_display == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_display_get");
		ret = false;
		goto finish;
	}

	//2.tpl_surface_create
	wnd->tpl_surf = NULL;
	wnd->tpl_surf = tpl_surface_create(wnd->tpl_display , (tpl_handle_t)wnd->wnd,
					   TPL_SURFACE_TYPE_WINDOW, TPL_FORMAT_ARGB8888);
	if (wnd->tpl_surf == NULL) {
		LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_create");
		ret = false;
		goto finish;
	}

	tpl_buffer_t *buf_array[STRESS_NUM] = {0};
	for (index = 0; index < STRESS_NUM; index++) {
		tpl_surface_begin_frame(wnd->tpl_surf);
		buf_array[index] = tpl_surface_get_buffer(wnd->tpl_surf, NULL);
		if (buf_array[index] == NULL) {
			LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_surface_get_buffer");
			ret = false;
			goto finish;
		}

		void *ptr = NULL;
		size = wnd->width * wnd->height ;
		ptr = tpl_buffer_map(buf_array[index], size);
		if (ptr == NULL) {
			LOG("ERRO", LOG_LEVEL_HIGH , "failed:tpl_buffer_map");
			ret = false;
			goto finish;
		}
		//6.write
		int *p = NULL;
		int j = 0;
		for (j = 0; j < size; j++) {
			p = (int *) ptr;
			if (index % 2 == 0)
				*(p + j) = 0xFF00;
			else
				*(p + j) = 0x00FF;
		}

		tpl_surface_end_frame(wnd->tpl_surf);

		tpl_surface_post(wnd->tpl_surf);
		usleep(500 * 1000); //0.5s
	}

finish:
	if (true == ret)
		LOG("PASS", LOG_LEVEL_HIGH , "Pass:%s", __func__);
	else
		LOG("FAIL", LOG_LEVEL_HIGH , "Failed:%s", __func__);
	return ret;
}


#endif


