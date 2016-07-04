#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include <unistd.h>

#include "tpl_test_util.h"

#include <wayland-client.h>
#include <wayland-egl.h>
#include <tbm_surface.h>


typedef struct _ProgOption ProgOption;

typedef struct _ProgOption {
	int egl_r;
	int egl_g;
	int egl_b;
	int egl_a;
	int egl_d;
	int egl_s;
	int egl_preserved;
	int egl_swap_interval;
	int wnd_x;
	int wnd_y;
	int wnd_w;
	int wnd_h;
	int frames;
	int tc_num;
	int fps;
	bool all;
	bool show_names;
};

ProgOption g_option = {
	8, 8, 8, 8,
	24,
	0,
	0,
	1,
	0, 0,
	1920, 1080,
	1000,
	0,
	300,
	false,
	false
};

/* log related */
#ifdef TPL_ENABLE_LOG
bool
__LOG( const char *func, int line, const char *fmt, ... )
{
	char buff[NUM_ERR_STR] = { (char)0, };
	char str[NUM_ERR_STR] = { (char)0, };
	va_list args;

	va_start( args, fmt );
	vsprintf( buff, fmt, args );
	va_end( args );

	sprintf( str, "[tpl_test] %s[%d] %s", func, line, buff );
	printf( "ddk:[tpl_test] %s[%d] %s\n", func, line, buff );
	__tpl_test_log_display_msg( str );

	return true;
}

void
__LOG_BEGIN( const char *func )
{
	char str[NUM_ERR_STR] = { (char)0, };
	sprintf( str, "[tpl_test][B] %s", func );
	printf( "ddk:[tpl_test][B] %s\n", func );
	__tpl_test_log_display_msg( str );
}

void
__LOG_END( const char *func )
{
	char str[NUM_ERR_STR] = { (char)0, };
	sprintf( str, "[tpl_test][E] %s", func );
	printf( "ddk:[tpl_test][E] %s\n", func );
	__tpl_test_log_display_msg( str );
}

bool
__tpl_test_log_display_msg( const char *msg )
{
	fprintf( DEFAULT_LOG_STREAM, "%s\n", msg );
	fflush( DEFAULT_LOG_STREAM );
	return 1;
}

bool
__LOG_ERR( const char *func, int line, const char *fmt, ... )
{
	char buff[NUM_ERR_STR] = { (char)0, };
	char str[NUM_ERR_STR] = { (char)0, };
	va_list args;

	va_start( args, fmt );
	vsprintf( buff, fmt, args );
	va_end( args );

	sprintf( str, "[tpl_test] %s[%d] %s", func, line, buff );

	__tpl_test_log_display_msg( str );

	return true;
}

#endif /* TPL_ENABLE_LOG */

/* wayland native related */
registry_handle_global(void *data, struct wl_registry *registry, uint32_t id,
		       const char *interface, uint32_t version)
{
	TPLNativeWnd *that = (TPLNativeWnd *)(data);

	if (strcmp(interface, "wl_compositor") == 0) {
		that->compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 1);
	} else if (strcmp(interface, "wl_shell") == 0) {
		that->shell = wl_registry_bind(registry, id, &wl_shell_interface, 1);
	} else if (strcmp(interface, "wl_output") == 0) {
		/*struct my_output *my_output = new struct my_output;
		memset(my_output, 0, sizeof(*my_output));
		my_output->output = wl_registry_bind(registry,id, &wl_output_interface, 2);*/
	}
}

void
registry_handle_global_remove(void *data, struct wl_registry *registry,
			      uint32_t name)
{
}

const struct wl_registry_listener registry_listener_ = {
	registry_handle_global,
	registry_handle_global_remove
};

shell_surface_handle_ping(void *data, struct wl_shell_surface *shell_surface,
			  uint32_t serial)
{
	wl_shell_surface_pong(shell_surface, serial);
}

void
shell_surface_handle_popup_done(void *data,
				struct wl_shell_surface *shell_surface)
{
}

void
shell_surface_handle_configure(void *data,
			       struct wl_shell_surface *shell_surface, uint32_t edges, int32_t width,
			       int32_t height)
{
	TPLNativeWnd *that = (TPLNativeWnd *)(data);
	that->width = width;
	that->height = height;
	wl_egl_window_resize(that->wnd, width, height, 0, 0);
}


const struct wl_shell_surface_listener shell_surface_listener_ = {
	shell_surface_handle_ping,
	shell_surface_handle_configure,
	shell_surface_handle_popup_done
};
/* wayland native end */

/* tpl_test related */
TPLNativeWnd *
tpl_test_native_wnd_create( void )
{
	TPLNativeWnd *wnd = NULL;

	TPL_RSM_MALLOC( wnd, TPLNativeWnd );

	//wnd->dpy = (NativeDisplayType)NULL;
	wnd->dpy = (void *)NULL;
	wnd->screen = 0;
	//wnd->root = (NativeWindowType)NULL;
	wnd->root = (void *)NULL;
	//wnd->wnd = (NativeWindowType)NULL;
	wnd->wnd = (void *)NULL;
	wnd->x = 0;
	wnd->y = 0;
	wnd->width = 0;
	wnd->height = 0;
	wnd->depth = 0;
	tpl_display_t *tpl_display = NULL;
	tpl_surface_t *tpl_surf = NULL;
finish:
	return wnd;
}

bool
tpl_test_native_wnd_initialize( TPLNativeWnd *wnd, int x, int y, int width,
				int height )
{
	bool res = false;

	TPL_CHK_PARAM( !wnd );
	TPL_CHK_PARAM( x < 0 );
	TPL_CHK_PARAM( y < 0 );
	TPL_CHK_PARAM( width <= 0 );
	TPL_CHK_PARAM( height <= 0 );

	//wnd->dpy = (NativeDisplayType)wl_display_connect(NULL);
	wnd->dpy = (void *)wl_display_connect(NULL);
	if ( !wnd->dpy ) {
		__log_err( "wl_display_connect() is failed.");
		return res;
	}

	wnd->registry = wl_display_get_registry(wnd->dpy);

	wl_registry_add_listener(wnd->registry, &registry_listener_, wnd);

	wl_display_roundtrip(wnd->dpy);

	wnd->x = x;
	wnd->y = y;
	wnd->width = width;
	wnd->height = height;
	wnd->depth = 32;

	wnd->surface = wl_compositor_create_surface(wnd->compositor);
	wnd->wnd = wl_egl_window_create(wnd->surface, wnd->width, wnd->height);

	wnd->shell_surface = wl_shell_get_shell_surface(wnd->shell, wnd->surface);

	wl_shell_surface_set_toplevel(wnd->shell_surface);
	if (wnd->shell_surface) {
		wl_shell_surface_add_listener(wnd->shell_surface, &shell_surface_listener_,
					      wnd);
	}

	wl_shell_surface_set_title(wnd->shell_surface, "tpl_testtest");

	res = true;
finish:
	return res;
}

bool
tpl_test_finalize( TPLNativeWnd *wnd )
{
	bool res = true;

	TPL_CHK_PARAM( !wnd );

	if (NULL != wnd->tpl_surf) {
		tpl_object_unreference((tpl_object_t *)wnd->tpl_surf);
	}
	if (NULL != wnd->tpl_display) {
		tpl_object_unreference((tpl_object_t *)wnd->tpl_display);
	}


	res = true;
finish:
	return res;
}

bool
tpl_test_native_wnd_finalize( TPLNativeWnd *wnd )
{
	bool res = false;

	TPL_CHK_PARAM( !wnd );
	TPL_CHK_PARAM( !wnd->dpy );

	wl_egl_window_destroy(wnd->wnd);
	if (wnd->shell_surface)
		wl_shell_surface_destroy(wnd->shell_surface);
	if (wnd->surface)
		wl_surface_destroy(wnd->surface);
	if (wnd->shell)
		wl_shell_destroy(wnd->shell);
	if (wnd->compositor)
		wl_compositor_destroy(wnd->compositor);

	wl_registry_destroy(wnd->registry);
	wl_display_flush(wnd->dpy);
	wl_display_disconnect(wnd->dpy);

	res = true;
finish:
	return res;
}

void
tpl_test_native_wnd_release( TPLNativeWnd *wnd )
{
	TPL_CHK_PARAM( !wnd );

	TPL_RSM_FREE( wnd );
finish:
	return;
}

void init_option()
{
	g_option.egl_r = 8;
	g_option.egl_r = 8;
	g_option.egl_g = 8;
	g_option.egl_b = 8;
	g_option.egl_a = 8;
	g_option.egl_d = 24;
	g_option.egl_s = 0;
	g_option.egl_preserved = 0;
	g_option.egl_swap_interval = 1;
	g_option.wnd_x = 0;
	g_option.wnd_y = 0;
	g_option.wnd_w = 1920;
	g_option.wnd_h = 1080;
	g_option.frames = 10000;
	g_option.tc_num = 0;
	g_option.fps = 300;
	g_option.all = true;
	g_option.show_names = false;
}
static void
print_usage( char *name )
{
	fprintf( stderr, "\n" );
	fprintf( stderr, "Usage: %s [OPTION]...\n", name );
	fprintf( stderr, "\n" );
	fprintf( stderr, "TPL test program for the libtpl-egl\n");
	fprintf( stderr, "  Build: " BLD_HOST_NAME " %s %s\n", __DATE__, __TIME__ );
	fprintf( stderr, "\n" );
	fprintf( stderr, "Options:\n" );

	fprintf( stderr, "  -w	Set width size of the window		 default: %d\n",
		 g_option.wnd_w );
	fprintf( stderr, "  -h	Set height size of the window		 default: %d\n",
		 g_option.wnd_h );

	fprintf( stderr, "  -t	Specify the test case number		 default: %d\n",
		 g_option.tc_num );
	fprintf( stderr, "  -a	Run all test cases			 default: %s\n",
		 g_option.all ? "true" : "false" );
	fprintf( stderr, "  -l	Show TC name				 default: %s\n",
		 g_option.show_names ? "true" : "false" );
	fprintf( stderr, "\n" );
	exit( 1 );
}

static void
check_option( int argc, char **argv )
{
	int c;
	char *opt_str = NULL;

	while ( (c = getopt(argc, argv, "alc:d:s:p:x:y:w:h:f:t:F:i:")) != EOF ) {
		switch ( c ) {
		case 'c':
			opt_str = optarg;
			if ( strcmp(opt_str, "888") == 0 ) {
				g_option.egl_r = 8;
				g_option.egl_g = 8;
				g_option.egl_b = 8;
				g_option.egl_a = 0;
			} else if ( strcmp(opt_str, "8888") == 0 ) {
				g_option.egl_r = 8;
				g_option.egl_g = 8;
				g_option.egl_b = 8;
				g_option.egl_a = 8;
			} else if ( strcmp(opt_str, "565") == 0 ) {
				g_option.egl_r = 5;
				g_option.egl_g = 6;
				g_option.egl_b = 5;
				g_option.egl_a = 0;
			}
			break;
		case 'd':
			g_option.egl_d = atol( optarg );
			break;
		case 's':
			g_option.egl_s = atol( optarg );
			break;
		case 'p':
			g_option.egl_preserved = atol( optarg );
			break;
		case 'i':
			g_option.egl_swap_interval = atol( optarg );
			break;
		case 'x':
			g_option.wnd_x = atol( optarg );
			break;
		case 'y':
			g_option.wnd_y = atol( optarg );
			break;
		case 'w':
			g_option.wnd_w = atol( optarg );
			break;
		case 'h':
			g_option.wnd_h = atol( optarg );
			break;
		case 'f':
			g_option.frames = atol( optarg );
			break;
		case 't':
			g_option.tc_num = atol( optarg );
			g_option.all = false;
			break;
		case 'F':
			g_option.fps = atol( optarg );
			break;
		case 'a':
			g_option.all = true;
			break;
		case 'l':
			g_option.show_names = true;
			break;
		default:
			print_usage( argv[0] );
			break;
		}
	}

}

int tpl_test_log_level = 1;

int
main( int argc, char **argv )
{
	TPLNativeWnd *wnd = NULL;
	bool res = false;
	int i = 0;
	int k = 0;
	int total_num_test = ( sizeof(tpl_test) / sizeof(TPLTest) ) - 1;
	int tc_num = 0;
	char *log_env;
	char *env;

	init_option();
	check_option( argc, argv );

	log_env = getenv("TPL_TEST_LOG_LEVEL");
	if (log_env != NULL) {
		tpl_test_log_level = atoi(log_env);
	}

	LOG("INFO", LOG_LEVEL_LOW, "tpl_test_log_level = %d", tpl_test_log_level);

	env = getenv("TEST_SLEEP");
	if (env && !strcmp(env, "yes")) {
		while (k < 20) {
			usleep(1000 * 1000);
			printf("sleep %d\n", k++);
		}
	}

	if ( g_option.show_names ) {
		printf( "-------------------------------------------------\n" );
		printf( "  number of test cass: %d		       \n", total_num_test );
		printf( "-------------------------------------------------\n" );
		while ( tpl_test[i].name ) {
			printf( "  [%2d] %s\n", i, tpl_test[i].name );
			i++;
		}
		printf( "-------------------------------------------------\n" );

		goto finish;
	}

	wnd = tpl_test_native_wnd_create();
	if ( !wnd ) goto finish;

	res = tpl_test_native_wnd_initialize( wnd,
					      g_option.wnd_x,
					      g_option.wnd_y,
					      g_option.wnd_w,
					      g_option.wnd_h );
	if ( !res ) goto finish;

	printf( "-------------------tpl test begin!!!-----------------------------------\n");

	if ( g_option.all ) {
		i = 0;

		while ( tpl_test[i].name ) {
			printf( "[%4d] %-50s", i, tpl_test[i].name );

			if ( tpl_test[i].run ) {
				if (true == tpl_test[i].run( wnd ))
					printf("[PASS]\n");
				else
					printf("[FAIL]\n");
			}
			i++;
		}
	} else {
		tc_num = g_option.tc_num;

		if ( tc_num < 0 || tc_num > total_num_test - 1 ) goto finish;

		//printf( "----------------------------------------------\n\n" );
		if ( tpl_test[tc_num].name ) printf( "[%4d] %-50s", tc_num,
							     tpl_test[tc_num].name );
		else printf( "[%4d] No test name\n", tc_num );

		if ( tpl_test[tc_num].run ) {
			if (true == tpl_test[tc_num].run( wnd ))
				printf("[PASS]\n");
			else
				printf("[FAIL]\n");
		}
		//printf( "\n----------------------------------------------\n\n" );
	}

	printf("-------------------tpl test end!!!-------------------------------------\n");

finish:
	if ( wnd ) {
		tpl_test_finalize( wnd );
		tpl_test_native_wnd_finalize( wnd );
		tpl_test_native_wnd_release( wnd );
	}


	return 0;
}

