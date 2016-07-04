#ifndef __TPL_UTIL__
#define __TPL_UTIL__

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>
//#include <GLES2/gl2.h>
//#include <GLES2/gl2ext.h>
//#include <EGL/egl.h>
//#include <EGL/eglext.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <tbm_surface.h>
#include <gbm.h>


//for tpl test:
#include <tpl.h>
//#include <tpl_internal.h>

#define STRESS_NUM   100

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _TPLNativeWnd TPLNativeWnd;


struct _TPLNativeWnd {
	void *dpy;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shell *shell;
	int screen;

	void *root;
	void *wnd;
	struct wl_surface *surface;
	struct wl_shell_surface *shell_surface;
	int x;
	int y;
	int width;
	int height;
	int depth;
	tpl_display_t *tpl_display;
	tpl_surface_t *tpl_surf;
    tbm_surface_h tbm_surf;
};

typedef struct _TPLTest TPLTest;

struct _TPLTest {
	char *name;
	bool (*run) (TPLNativeWnd *);
};

extern int tpl_test_log_level;
#define minLevel tpl_test_log_level
#define LOG_LEVEL_LOW 1
#define LOG_LEVEL_MID 2
#define LOG_LEVEL_HIGH 3

#define LOG(type,level, format, ...) \
    do { \
       if(level>=minLevel) fprintf(stderr, "[%s|%22s:%3d] " format "\n", \
		type, __FILE__, __LINE__, ##__VA_ARGS__ ); \
    } while (0)

#define LONGLOG(type,level, format, ...) \
    do { \
       if(level>=minLevel) fprintf(stderr, "[%s|%s@%s:%d] " format "\n", \
		type, __func__, __FILE__, __LINE__, ##__VA_ARGS__ ); \
    } while (0)


/* for tpl test */
bool tpl_display_create_test (TPLNativeWnd *wnd);
bool tpl_display_get_test (TPLNativeWnd *wnd);
bool tpl_display_get_native_handle_test (TPLNativeWnd *wnd);
bool tpl_display_query_config_test (TPLNativeWnd *wnd);
bool tpl_display_filter_config_test (TPLNativeWnd *wnd);

bool tpl_object_get_type_test(TPLNativeWnd* wnd);
bool tpl_object_userdata_test(TPLNativeWnd* wnd);
bool tpl_object_reference_test(TPLNativeWnd* wnd);

bool tpl_surface_create_test(TPLNativeWnd* wnd);
bool tpl_surface_validate_test(TPLNativeWnd* wnd);
bool tpl_surface_get_arg_test(TPLNativeWnd* wnd);
bool tpl_surface_dequeue_and_enqueue_buffer_test(TPLNativeWnd* wnd);


static TPLTest tpl_test[] = {
	{ "TPL display create test", tpl_display_create_test },
	{ "TPL display get test", tpl_display_get_test },
    { "TPL display get native handle test", tpl_display_get_native_handle_test },	
    { "TPL display query config test", tpl_display_query_config_test },	
    { "TPL display filter config test", tpl_display_filter_config_test },	

    { "TPL object get types test", tpl_object_get_type_test },
    { "TPL object set and get userdata test", tpl_object_userdata_test },
    { "TPL object reference and unreference test", tpl_object_reference_test },

    { "TPL surface create test", tpl_surface_create_test },
    { "TPL surface validate test", tpl_surface_validate_test },
    { "TPL surface get args test", tpl_surface_get_arg_test },
    { "TPL surface dequeue and buffer test", tpl_surface_dequeue_and_enqueue_buffer_test },

    { NULL, NULL }

};

#define TPL_RESOURCE_BIN_SHADER_VTX_01 "data/01_vtx.bin"
#define TPL_RESOURCE_BIN_SHADER_FRAG_01 "data/01_frag.bin"
#define TPL_RESOURCE_BIN_PROGRAM_01 "data/01_program.bin"
#define TPL_RESOURCE_TGA_UNCOMP_FILE_01 "data/sample_01_uncomp.tga"
#define TPL_RESOURCE_TGA_COMP_FILE_02 "data/sample_02_comp.tga"


/*-----------------------------------------------------------------
 * time
 *-----------------------------------------------------------------*/
#define HAVE_MONOTONIC_CLOCK 1
#define __SEC_TO_USEC( sec ) ((sec) * 1000000)
#define __USEC_TO_SEC( usec ) ((float)(usec) * 1.0e-6f)
#define __MSEC_TO_SEC( usec ) ((float)(usec) * 1.0e-3f)
long int tpl_test_util_get_systime( void );
void tpl_test_util_init_fps( long int *s_time );
float tpl_test_util_get_fps( long int s_time, int frame );

/*-----------------------------------------------------------------
 * performance measurement
 *-----------------------------------------------------------------*/
#define TPL_MEASURE_PERF 1

#if TPL_MEASURE_PERF
# include <sys/times.h>

typedef struct _GfxUtilTimer GfxUtilTimer;

#define USE_GETTIME 1
struct _GfxUtilTimer {
	bool is_begin;
	bool is_measured;
	char func[1024];
	int line;
	char msg[1024];
#if USE_GETTIME
	long int begin_t;
	long int end_t;
#else
	clock_t begin_tiks;
	clock_t end_tiks;
	struct tms begin_buf;
	struct tms end_buf;
#endif
};

# define __TPL_TIMER_GLOBAL_BEGIN( timer, msg ) \
	tpl_test_util_timer_begin( timer, __func__, __LINE__, msg );
# define __TPL_TIMER_GLOBAL_END( timer, msg ) \
	tpl_test_util_timer_end( timer, __func__, __LINE__, msg );

# define __TPL_TIMER_BEGIN( msg ) \
	GfxUtilTimer __timer = { false, 0, 0 }; \
	tpl_test_util_timer_begin( &__timer, __func__, __LINE__, msg );
# define __TPL_TIMER_END( msg ) \
	tpl_test_util_timer_end( &__timer, __func__, __LINE__, msg );

void		tpl_test_util_timer_list_display( void );
void		tpl_test_util_timer_release( GfxUtilTimer *timer );
GfxUtilTimer	*tpl_test_util_timer_copy( GfxUtilTimer *src, const char *func,
		int line, const char *msg );
void		tpl_test_util_timer_list_clear( void );
void		tpl_test_util_timer_begin( GfxUtilTimer *timer, const char *func,
		int line, const char *msg );
void		tpl_test_util_timer_end( GfxUtilTimer *timer, const char *func, int line,
		const char *msg );
#else
# define __TPL_TIMER_GLOBAL_BEGIN( ... ) { ; }
# define __TPL_TIMER_GLOBAL_END( ... ) { ; }
# define __TPL_TIMER_BEGIN( ... ) { ; }
# define __TPL_TIMER_END( ... ) { ; }
# define tpl_test_util_timer_list_display( ... ) { ; }
# define tpl_test_util_timer_list_clear( ... ) { ; }
#endif


/*-----------------------------------------------------------------
 * log
 *-----------------------------------------------------------------*/

#define TPL_ENABLE_LOG 1




#define NUM_ERR_STR 512 /* length of the error logging string */
#define DEFAULT_LOG_STREAM stderr

#define __log_err(fmt, args...) __LOG_ERR(__func__, __LINE__, fmt, ##args)
bool __LOG_ERR( const char *func, int line, const char *fmt, ... );
bool __tpl_test_log_display_msg( const char *msg );



#if TPL_ENABLE_LOG
# define __log(fmt, args...) __LOG(__func__, __LINE__, fmt, ##args)
# define __log_begin( ... ) __LOG_BEGIN(__func__);
# define __log_end( ... ) __LOG_END(__func__);
bool __LOG( const char *func, int line, const char *fmt, ... );
void __LOG_BEGIN( const char *func );
void __LOG_END( const char *func );
#else
# define __log( ... ) { ; }
# define __log_begin( ... ) { ; }
# define __log_end( ... ) { ; }
# define __LOG( ... ) { ; }
# define __LOG_BEGIN( ... ) { ; }
# define __LOG_END( ... ) { ; }
#endif


/*-----------------------------------------------------------------
 * validation
 *-----------------------------------------------------------------*/
#define TPL_RSM_MALLOC( obj, type ) \
	{ \
		obj = (type*)malloc( sizeof(type) ); \
		if( !obj ) \
		{ \
			__log_err( "failed to allocate memory" ); \
			goto finish; \
		} \
	}

#define TPL_RSM_FREE( obj ) \
	{ \
		free( obj ); \
	}

#define TPL_RSM_MEMCPY( dst, src, type, num ) \
	{ \
		memcpy( dst, src, sizeof(type) * num ); \
	}

#define	TPL_CHK_PARAM( exp ) \
	{ \
		if( exp ) \
		{ \
			__log_err( "invalid input parameter. '"#exp"' is true" ); \
			goto finish; \
		} \
	}

/*-----------------------------------------------------------------
 * math and misc
 *-----------------------------------------------------------------*/
#define PI 3.1415926535897932384626433832795f

#define FLOAT_TO_FIXED(x) (long)((x)*65536.0f)

typedef struct _GfxMatrix GfxMatrix_t;

struct _GfxMatrix {
	//GLfloat m[4][4];
};

typedef enum {
	TPL_TEX_COLOR_BLACK,
	TPL_TEX_COLOR_WHITE,
	TPL_TEX_COLOR_GREY,
	TPL_TEX_COLOR_RED,
	TPL_TEX_COLOR_GREEN,
	TPL_TEX_COLOR_BLUE,
	TPL_TEX_COLOR_RGB,
	TPL_TEX_COLOR_RGB2,
	TPL_TEX_COLOR_RGBA_TRANSLUCENCE,
	TPL_TEX_COLOR_RGBA_4444,
	TPL_TEX_COLOR_RGBA_5551,
} GfxTexColor;

#ifdef __cplusplus
}
#endif

#endif /* __TPL_UTIL__ */
