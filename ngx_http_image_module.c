/*
* Copyright (C) Vampire
*
* {根据URL生成缩略图/添加水印}
*
* nginx.conf 配置值
* image on/off 是否开启缩略图功能,默认关闭
* image_backend on/off 是否开启镜像服务
* image_backend_server 镜像服务器地址
* image_output on/off 是否不生成图片而直接处理后输出 默认off
* image_jpeg_quality 75 生成JPEG图片的质量 默认值75
* image_water on/off 是否开启水印功能
* image_water_type 0/1 水印类型 0:图片水印 1:文字水印
* image_water_min 300 300 图片宽度 300 高度 300 的情况才添加水印
* image_water_pos 0-9 水印位置 默认值9 0为随机位置,1为顶端居左,2为顶端居中,3为顶端居右,4为中部居左,5为中部居中,6为中部居右,7为底端居左,8为底端居中,9为底端居右
* image_water_file 水印文件(jpg/png/gif),绝对路径或者相对路径的水印图片
* image_water_transparent 水印透明度,默认20
* image_water_text 水印文字 "Power By Vampire"
* image_water_font_size 水印大小 默认 5
* image_water_font;//文字水印字体文件路径
* image_water_color 水印文字颜色,默认 #000000
*/

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <string.h>
#include <gd.h>
#include <gdfontg.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pcre.h>
#include <curl/curl.h>
#include "ngx_http_request.h"

#define NGX_IMAGE_NONE      0
#define NGX_IMAGE_JPEG      1
#define NGX_IMAGE_GIF       2
#define NGX_IMAGE_PNG       3
#define NGX_IMAGE_BMP       4

#ifndef WIN32
#define stricmp strcasecmp
#endif

typedef struct
{
    ngx_flag_t image_status;//是否打开图片处理
	char * url;//请求URL地址
	char * request_dir;//URL目录
	char * request_source;//URL源文件URL
	char * request_filename;//URL中的文件名
	char * local_dir;//当前WEB目录
	char * extension;//目标图片后缀名 (png/gif/jpg/jpeg/jpe)
	char * m_type;//生成缩略图的方式 缩放/居中缩放/顶部10%开始缩放
	char * source_file;//原始图片路径
	char * dest_file;//目标图片路径
	u_char * img_data;//图片内容
	gdImagePtr src_im;//原始图片GD对象
	gdImagePtr dst_im;//目标图片GD对象
    gdImagePtr w_im;//补白边图片GD对象
    int w_margin;//是否对图片补白边
	int img_size;//图片大小
	int pcre_type;//图片正则匹配类型 0为新规则(test.jpg!c300x300.jpg) 1为旧规则(test.c300x300.jpg)
	int header_type;//HTTP头部类型
	int max_width;//目标图片最大宽度
	int max_height;//目标图片最大宽度
	int src_type;//原始图片类型
	int dest_type;//目标图片类型
	int src_width;//原始图片宽度
	int src_height;//原始图片高度
	int src_x;//原始图片X坐标
	int src_y;//原始图片Y坐标
	int src_w;//原始图片宽度
	int src_h;//原始图片高度
	int width;//目标图片宽度
	int height;//目标图片高度
	int dst_x;//目标图片X坐标
    int dst_y;//目标图片Y坐标
	ngx_flag_t image_output;//是否不保存图片直接输出图片内容给客户端 默认off
	int jpeg_quality;//JPEG图片质量 默认75
	gdImagePtr water_im;//水印图片GD对象
	ngx_flag_t water_status;//是否打开水印功能 默认关闭
	int water_im_type;//水印图片类型
	int water_type;//水印类型 0:图片水印 1:文字水印
	int water_pos;//水印位置
	int water_transparent;//水印透明度
	int water_width_min;//原图小于该宽度的图片不添加水印
	int water_height_min;//原图小于该高度的图片不添加水印
	ngx_flag_t backend;//是否请求原始服务器
	ngx_str_t backend_server;//图片原始服务器,如请求的图片不存在,从该服务器下载
	ngx_str_t water_image;//水印图片
	ngx_str_t water_text;//水印文字内容
	int water_font_size;//水印文字大小
	ngx_str_t water_font;//文字水印字体文件路径
	ngx_str_t water_color;//水印文字颜色 (#0000000)
} ngx_image_conf_t;

static FILE *curl_handle;

static ngx_str_t  ngx_http_image_types[] =
{
        ngx_string("text/html"),
		ngx_string("image/jpeg"),
		ngx_string("image/gif"),
		ngx_string("image/png")
};

static char *ngx_http_image(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static void *ngx_http_image_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_image_merge_loc_conf(ngx_conf_t *cf,void *parent, void *child);
static char * ngx_http_image_water_min(ngx_conf_t *cf, ngx_command_t *cmd,void *conf);
static ngx_uint_t ngx_http_image_value(ngx_str_t *value);
char * ngx_conf_set_number_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
char * ngx_conf_set_string_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t output(ngx_http_request_t *r,void *conf,ngx_str_t type);
static void gd_clean_data(void *data);//清除GD DATA数据
static void make_thumb(void *conf);//创建GD对象缩略图,缩略图在此函数中已经处理好,但没有写入到文件
static void water_mark(void *conf);//给图片打上水印
static void thumb_to_string(void *conf);//GD对象数据转换为二进制字符串
static int parse_image_info(void *conf);//根据正则获取基本的图片信息
static int calc_image_info(void *conf);//根据基本图片信息计算缩略图信息
static void check_image_type(void *conf);//判断图片类型
static char * get_ext(char *filename);//根据文件名取图片类型
static int get_ext_header(char *filename);//根据文件头取图片类型
static int file_exists(char *filename);//判断文件是否存在
static int read_img(char **filename,int * size,void ** buffer);//图片读入函数
static void write_img(void * conf);//图片保存到文件
static void water_image_from(void * conf);//创建水印图片GD对象
static void image_from(void * conf);//创建原图GD库对象
static int get_header(char *url);//取得远程URL的返回值
static size_t curl_get_data(void *ptr, size_t size, size_t nmemb, void *stream);//CURL调用函数
static int create_dir(char *dir);//递归创建目录
static void get_request_source(void *conf);
static void dirname(char *path,char *dirpath);//根据URL获取目录路径
static void download(void * conf);//下载文件


static ngx_command_t  ngx_http_image_commands[] =
{
	{
		ngx_string("image"),
			NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
			ngx_http_image,
			NGX_HTTP_LOC_CONF_OFFSET,
			0,
			NULL
	},
	{
		ngx_string("image_output"),
			NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
			ngx_conf_set_flag_slot,
			NGX_HTTP_LOC_CONF_OFFSET,
			offsetof(ngx_image_conf_t, image_output),
			NULL
	},
		{
			ngx_string("image_backend"),
				NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
				ngx_conf_set_flag_slot,
				NGX_HTTP_LOC_CONF_OFFSET,
				offsetof(ngx_image_conf_t, backend),
				NULL
		},
		{
			ngx_string("image_backend_server"),
				NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
				ngx_conf_set_str_slot,
				NGX_HTTP_LOC_CONF_OFFSET,
				offsetof(ngx_image_conf_t, backend_server),
				NULL
		},
			{
				ngx_string("image_water"),
					NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
					ngx_conf_set_flag_slot,
					NGX_HTTP_LOC_CONF_OFFSET,
					offsetof(ngx_image_conf_t, water_status),
					NULL
			},
			{
				ngx_string("image_water_type"),
					NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_ARGS_NUMBER,
					ngx_conf_set_number_slot,
					NGX_HTTP_LOC_CONF_OFFSET,
					offsetof(ngx_image_conf_t, water_type),
					NULL
			},
				{
					ngx_string("image_water_min"),
						NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE2,
						ngx_http_image_water_min,
						NGX_HTTP_LOC_CONF_OFFSET,
						0,
						NULL
				},
				{
					ngx_string("image_water_pos"),
						NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_ARGS_NUMBER,
						ngx_conf_set_number_slot,
						NGX_HTTP_LOC_CONF_OFFSET,
						offsetof(ngx_image_conf_t, water_pos),
						NULL
				},
					{
						ngx_string("image_water_file"),
							NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
							ngx_conf_set_str_slot,
							NGX_HTTP_LOC_CONF_OFFSET,
							offsetof(ngx_image_conf_t, water_image),
							NULL
					},
					{
						ngx_string("image_water_transparent"),
							NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
							ngx_conf_set_number_slot,
							NGX_HTTP_LOC_CONF_OFFSET,
							offsetof(ngx_image_conf_t, water_transparent),
							NULL
					},
						{
							ngx_string("image_water_text"),
								NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
								ngx_conf_set_str_slot,
								NGX_HTTP_LOC_CONF_OFFSET,
								offsetof(ngx_image_conf_t, water_text),
								NULL
						},
						{
							ngx_string("image_water_font"),
								NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
								ngx_conf_set_str_slot,
								NGX_HTTP_LOC_CONF_OFFSET,
								offsetof(ngx_image_conf_t, water_font),
								NULL
						},
							{
								ngx_string("image_water_font_size"),
									NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_ARGS_NUMBER,
									ngx_conf_set_number_slot,
									NGX_HTTP_LOC_CONF_OFFSET,
									offsetof(ngx_image_conf_t, water_font_size),
									NULL
							},
							{
								ngx_string("image_water_color"),
									NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
									ngx_conf_set_str_slot,
									NGX_HTTP_LOC_CONF_OFFSET,
									offsetof(ngx_image_conf_t, water_color),
									NULL
							},
								{
									ngx_string("image_jpeg_quality"),
										NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_ARGS_NUMBER,
										ngx_conf_set_number_slot,
										NGX_HTTP_LOC_CONF_OFFSET,
										offsetof(ngx_image_conf_t, jpeg_quality),
										NULL
								},
								ngx_null_command
};


static ngx_http_module_t  ngx_http_image_module_ctx =
{
	NULL,                          /* preconfiguration */
		NULL,                          /* postconfiguration */

		NULL,                          /* create main configuration */
		NULL,                          /* init main configuration */

		NULL,                          /* create server configuration */
		NULL,                          /* merge server configuration */

		ngx_http_image_create_loc_conf,                          /* create location configuration */
		ngx_http_image_merge_loc_conf                           /* merge location configuration */
};


ngx_module_t  ngx_http_image_module =
{
	NGX_MODULE_V1,
		&ngx_http_image_module_ctx,      /* module context */
		ngx_http_image_commands,         /* module directives */
		NGX_HTTP_MODULE,               /* module type */
		NULL,                          /* init master */
		NULL,                          /* init module */
		NULL,                          /* init process */
		NULL,                          /* init thread */
		NULL,                          /* exit thread */
		NULL,                          /* exit process */
		NULL,                          /* exit master */
		NGX_MODULE_V1_PADDING
};

static void *
ngx_http_image_create_loc_conf(ngx_conf_t *cf)
{
	ngx_image_conf_t  *conf;

	conf = ngx_pcalloc(cf->pool, sizeof(ngx_image_conf_t));
	if (conf == NULL)
	{
		return NULL;
	}
	conf->image_status = NGX_CONF_UNSET;
	conf->backend = NGX_CONF_UNSET;
	conf->jpeg_quality = NGX_CONF_UNSET;
	conf->image_output = NGX_CONF_UNSET;
	conf->water_status = NGX_CONF_UNSET;
	conf->water_type = NGX_CONF_UNSET;
	conf->water_pos = NGX_CONF_UNSET;
	conf->water_transparent = NGX_CONF_UNSET;
	conf->water_width_min = NGX_CONF_UNSET;
	conf->water_height_min = NGX_CONF_UNSET;
	conf->water_font_size = NGX_CONF_UNSET;
	return conf;
}

static char *
ngx_http_image_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
	ngx_image_conf_t  *prev = parent;
	ngx_image_conf_t  *conf = child;
	ngx_conf_merge_value(conf->image_status,prev->image_status,0);
	ngx_conf_merge_value(conf->backend,prev->backend,0);
	ngx_conf_merge_str_value(conf->backend_server,prev->backend_server,"http://image.oupula.org/");
	ngx_conf_merge_value(conf->jpeg_quality,prev->jpeg_quality,75);
	ngx_conf_merge_value(conf->water_type,prev->water_type,0);
	ngx_conf_merge_value(conf->water_width_min,prev->water_width_min,0);
	ngx_conf_merge_value(conf->water_height_min,prev->water_height_min,0);
	ngx_conf_merge_value(conf->water_pos,prev->water_pos,9);
	ngx_conf_merge_value(conf->water_transparent,prev->water_transparent,20);
	ngx_conf_merge_str_value(conf->water_text,prev->water_text,"[ Copyright By Vampire ]");
	ngx_conf_merge_value(conf->water_font_size,prev->water_font_size,5);
	ngx_conf_merge_str_value(conf->water_font,prev->water_font,"/usr/share/fonts/truetype/wqy/wqy-microhei.ttc");
	ngx_conf_merge_str_value(conf->water_color,prev->water_color,"#000000");
	return NGX_CONF_OK;
}


static ngx_int_t ngx_http_image_handler(ngx_http_request_t *r)
{
	u_char                    *last;
	size_t                     root;
	ngx_int_t                  rc;
	ngx_str_t                  path;
	char                       request_uri[255];
	int                        request_uri_len;
	ngx_image_conf_t  *conf;
	conf = ngx_http_get_module_loc_conf(r, ngx_http_image_module);
	if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD)))
	{
		return NGX_HTTP_NOT_ALLOWED;
	}
	if (r->headers_in.if_modified_since)
	{
		return NGX_HTTP_NOT_MODIFIED;
	}
	
	if (r->uri.data[r->uri.len - 1] == '/')
	{
		return NGX_DECLINED;
	}

	rc = ngx_http_discard_request_body(r);
	if (rc != NGX_OK)
	{
		return rc;
	}
	last = ngx_http_map_uri_to_path(r, &path, &root, 0);
	if (last == NULL)
	{
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}
	if(file_exists((char*) path.data) == -1)
	{
		request_uri_len = strlen((char *)r->uri_start) - strlen((char *)r->uri_end);
		strncpy(request_uri, (char *)r->uri_start, request_uri_len);
		request_uri[request_uri_len] = '\0';
		dirname(request_uri,conf->request_dir);
		conf->url = request_uri;//请求的URL地址
		conf->dest_file = (char *)path.data;
		check_image_type(conf);//检查图片类型(根据后缀进行简单判断)
		if( conf->dest_type > 0 )
		{

			if (parse_image_info(conf) == 0)//解析并处理请求的图片URL
			{

				make_thumb(conf);//生成图片缩略图
				water_mark(conf);//图片打上水印
				thumb_to_string(conf);//GD对象转换成二进制字符串
				if(conf->image_output == 0)
				{
					write_img(conf);//保存图片缩略图到文件
				}
				if(conf->image_output == 1)
				{
					return output(r,conf,ngx_http_image_types[conf->dest_type]);
				}
			}
		}
	}
	return NGX_DECLINED;
}

static char * ngx_http_image_water_min(ngx_conf_t *cf, ngx_command_t *cmd,void *conf)
{
	ngx_image_conf_t *info = conf;
	ngx_str_t                         *value;
	ngx_http_complex_value_t           cv;
	ngx_http_compile_complex_value_t   ccv;
	value = cf->args->elts;
	ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));
	ccv.cf = cf;
	ccv.value = &value[1];
	ccv.complex_value = &cv;
	if (ngx_http_compile_complex_value(&ccv) != NGX_OK)
	{
		return NGX_CONF_ERROR;
	}
	if (cv.lengths == NULL)
	{
		info->water_width_min = (int)ngx_http_image_value(&value[1]);
		info->water_height_min = (int)ngx_http_image_value(&value[2]);
	}
	return NGX_CONF_OK;
}

static ngx_uint_t ngx_http_image_value(ngx_str_t *value)
{
	ngx_int_t  n;
	if (value->len == 1 && value->data[0] == '-')
	{
		return (ngx_uint_t) -1;
	}
	n = ngx_atoi(value->data, value->len);
	if (n > 0)
	{
		return (ngx_uint_t) n;
	}
	return 0;
}

char * ngx_conf_set_number_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	char  *p = conf;
	int        *np;
	ngx_str_t        *value;
	np = (int *) (p + cmd->offset);
	if (*np != NGX_CONF_UNSET)
	{
		return "is duplicate";
	}
	value = cf->args->elts;
	*np = (int)ngx_atoi(value[1].data, value[1].len);
	if (*np == NGX_ERROR)
	{
		return "invalid number";
	}
	return NGX_CONF_OK;
}

static char *
ngx_http_image(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	ngx_str_t        *value;
	value = cf->args->elts;
	if (ngx_strcasecmp(value[1].data, (u_char *) "on") == 0)
	{
		ngx_http_core_loc_conf_t  *clcf;
		clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
		clcf->handler = ngx_http_image_handler;
		return NGX_CONF_OK;
	}
	else if (ngx_strcasecmp(value[1].data, (u_char *) "off") == 0)
	{
		return NGX_CONF_OK;
	}
	else
	{
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,"invalid value \"%s\" in \"%s\" directive, ""it must be \"on\" or \"off\"",value[1].data, cmd->name.data);
		return NGX_CONF_ERROR;
	}
	return NGX_CONF_OK;
}

static ngx_int_t output(ngx_http_request_t *r,void *conf,ngx_str_t type)
{
    ngx_int_t status = 0;
	ngx_image_conf_t *info = conf;
	ngx_http_complex_value_t  cv;
    ngx_pool_cleanup_t            *cln;
    cln = ngx_pool_cleanup_add(r->pool, 0);
    if (cln == NULL) {
        gdFree(info->img_data);
        return status;
    }
    cln->handler = gd_clean_data;
    cln->data = info->img_data;

	ngx_memzero(&cv, sizeof(ngx_http_complex_value_t));
	cv.value.len = info->img_size;
	cv.value.data = (u_char *)info->img_data;
    status = ngx_http_send_response(r, NGX_HTTP_OK, &type, &cv);
    return status;
}

static void thumb_to_string(void *conf)
{
	ngx_image_conf_t *info = conf;

	switch(info->dest_type)
	{
        case NGX_IMAGE_PNG:
            info->img_data = gdImagePngPtr(info->dst_im,&info->img_size);
            break;
        case NGX_IMAGE_GIF:
            info->img_data = gdImageGifPtr(info->dst_im,&info->img_size);
            break;
        case NGX_IMAGE_JPEG:
            info->img_data = gdImageJpegPtr(info->dst_im,&info->img_size,info->jpeg_quality);
            break;
    }
    gdImageDestroy(info->dst_im);
}

static void gd_clean_data(void *data){
    gdFree(data);
}

static void make_thumb(void *conf)
{
	int colors = 0;
	int transparent = -1;
	ngx_image_conf_t *info = conf;
	info->dst_im = gdImageCreateTrueColor(info->width,info->height);
	colors = gdImageColorsTotal(info->src_im);
	transparent = gdImageGetTransparent(info->src_im);
	if (transparent == -1)
        {
		gdImageSaveAlpha(info->src_im,1);
		gdImageColorTransparent(info->src_im, -1);
		if(colors == 0)
		{
			gdImageAlphaBlending(info->dst_im,0);
			gdImageSaveAlpha(info->dst_im,1);
		}
		if(colors)
		{
			gdImageTrueColorToPalette(info->dst_im,1,256);
		}
    }
    if(info->w_margin == 1)
    {

		info->w_im = gdImageCreateTrueColor(info->width,info->height);
		gdImageFilledRectangle(info->w_im, 0, 0, info->width,info->height, gdImageColorAllocate(info->w_im, 255, 255, 255));
        info->dst_im = gdImageCreateTrueColor(info->max_width,info->max_height);
        gdImageFilledRectangle(info->dst_im, 0, 0, info->max_width,info->max_height, gdImageColorAllocate(info->dst_im, 255, 255, 255));
		gdImageCopyResampled(info->w_im, info->src_im, 0, 0, info->src_x, info->src_y,info->width, info->height, info->src_w,info->src_h);
		gdImageCopyResampled(info->dst_im, info->w_im, info->dst_x,info->dst_y, 0, 0,info->width, info->height, info->width, info->height);
        gdImageDestroy(info->w_im);
    }
    else
    {

        gdImageCopyResampled(info->dst_im,info->src_im,info->dst_x,info->dst_y,info->src_x,info->src_y,info->width,info->height,info->src_w,info->src_h);
    }
    gdImageDestroy(info->src_im);
}
static void water_mark(void *conf)
{
	ngx_image_conf_t *info = conf;
	int water_w=0;//水印宽度
	int water_h=0;//水印高度
	int posX = 0;//X位置
	int posY = 0;//Y位置
	int water_color = 0;//文字水印GD颜色值
	char *water_text;//图片文字
	char *water_font;//文字字体
	char *water_color_text;//图片颜色值
	water_text = NULL;
	water_font = NULL;
	water_color_text = NULL;

	if(info->water_status)//如果水印功能打开了
	{

		if(info->water_type == 0)//如果为图片水印
		{
			if(file_exists((char *)info->water_image.data) == 0)//判断水印图片是否存在
			{
				water_image_from(conf);//获取水印图片信息
				if(info->water_im == NULL)//判断对象是否为空
				{
                    return;//水印文件异常
                }else{
                    water_w = info->water_im->sx;
                    water_h = info->water_im->sy;
                }
			}
			else
			{
				return;//水印图片不存在
			}
		}
		else//文字水印
		{
			water_text = (char *) info->water_text.data;
			water_color_text = (char *) info->water_color.data;
			water_font = (char *)info->water_font.data;
			if(file_exists((char *)water_font) == 0)//如果水印字体存在
			{
				int R,G,B;
				char R_str[3],G_str[3],B_str[3];
				int brect[8];
				gdImagePtr font_im;
				font_im = gdImageCreateTrueColor(info->dst_im->sx,info->dst_im->sy);
				sprintf(R_str,"%.*s",2,water_color_text+1);
				sprintf(G_str,"%.*s",2,water_color_text+3);
				sprintf(B_str,"%.*s",2,water_color_text+5);
				sscanf(R_str,"%x",&R);
				sscanf(G_str,"%x",&G);
				sscanf(B_str,"%x",&B);
				water_color = gdImageColorAllocate(info->dst_im,R,G,B);
				gdImageStringFT(font_im, &brect[0], water_color, water_font, info->water_font_size, 0.0, 0, 0,water_text/*, &strex*/);
				//water_w = abs(brect[2] - brect[6] + 10);
				water_w = abs(brect[2] - brect[6] + 10);
				water_h = abs(brect[3] - brect[7]);
				gdImageDestroy(font_im);
			}

		}
		if( (info->width < info->water_width_min) || info->height < info->water_height_min)
		{
			return;//如果图片宽度/高度比配置文件里规定的宽度/高度宽度小
		}
		if ((info->width < water_w) || (info->height < water_h))
		{
			return;//如果图片宽度/高度比水印宽度/高度宽度小
		}
		if(info->water_pos < 1 ||info->water_pos > 9)
		{
			srand((unsigned)time(NULL));
			//info->water_pos = rand() % 9 + 1;
			info->water_pos = 1+(int)(9.0*rand()/(RAND_MAX+1.0));
			//info->water_pos = rand() % 9;
		}
		switch(info->water_pos)
		{
		case 1:
			posX = 10;
			posY = 15;
			break;
		case 2:
			posX = (info->width - water_w) / 2;
			posY = 15;
			break;
		case 3:
			posX = info->width - water_w;
			posY = 15;
			break;
		case 4:
			posX = 0;
			posY = (info->height - water_h) / 2;
			break;
		case 5:
			posX = (info->width - water_w) / 2;
			posY = (info->height - water_h) / 2;
			break;
		case 6:
			posX = info->width - water_w;
			posY = (info->height - water_h) / 2;
			break;
		case 7:
			posX = 0;
			posY = (info->height - water_h);
			break;
		case 8:
			posX = (info->width - water_w) /2;
			posY = info->width - water_h;
			break;
		case 9:
			posX = info->width - water_w;
			posY = info->height - water_h;
			break;
		default:
			posX = info->width - water_w;
			posY = info->height - water_h;
			break;
		}
		if(info->water_type == 0)
		{
			gdImagePtr tmp_im;
			tmp_im = NULL;
			tmp_im = gdImageCreateTrueColor(water_w, water_h);
			gdImageCopy(tmp_im, info->dst_im, 0, 0, posX, posY, water_w, water_h);
			gdImageCopy(tmp_im, info->water_im, 0, 0, 0, 0, water_w, water_h);
			gdImageCopyMerge(info->dst_im, tmp_im,posX, posY, 0, 0, water_w,water_h,info->water_transparent);
			gdImageDestroy(tmp_im);
            gdImageDestroy(info->water_im);
		}
		else
		{
			gdImageAlphaBlending(info->dst_im,-1);
			gdImageSaveAlpha(info->dst_im,0);
			gdImageStringFT(info->dst_im,0,water_color,water_font,info->water_font_size, 0.0, posX, posY,water_text);
		}
	}
}

static int parse_image_info(void *conf)
{
	void *(*old_pcre_malloc)(size_t);
	void (*old_pcre_free)(void *);
	pcre *expr;//正则
	char *pattern;
    char buffer[6][255];
	const char *error;//正则错误内容
	int pcre_state=0;//匹配图片规则状态,0为成功 -1为失败
	int erroffset;//正则错误位置
	int ovector[30];//识别器读取原图图片到GD对象
	int expr_res;//正则匹配指针
	int i=0;//循环用
	ngx_image_conf_t *info = conf;
	info->request_filename = NULL;
	old_pcre_malloc = pcre_malloc;
	old_pcre_free = pcre_free;
	pcre_malloc = malloc;
	pcre_free = free;
	if(strchr(info->dest_file,'!'))
	{
		info->pcre_type = 0;
		//pattern = "([^<]*)!([a-z])(\\d{2,4})x(\\d{2,4}).([a-zA-Z]{3,4})";//正则表达式
		pattern = "([^<]*)\\/([^<]*)!([a-z])(\\d{2,4})x(\\d{2,4}).([a-zA-Z]{3,4})";//正则表达式
	}
	else
	{
		info->pcre_type = 1;
		//pattern = "([^<]*).([a-z])(\\d{2,4})x(\\d{2,4}).([a-zA-Z]{3,4})";//正则表达式
		pattern = "([^<]*)\\/([^<]*).([a-z])(\\d{2,4})x(\\d{2,4}).([a-zA-Z]{3,4})";//正则表达式
	}
	expr = pcre_compile((const char *)pattern,0,&error,&erroffset,NULL);
	if(expr != NULL)
	{
		expr_res = pcre_exec(expr,NULL,(const char *)info->dest_file,ngx_strlen(info->dest_file),0,0,ovector,30);
		if(expr_res > 5)
		{
			for(i=0; i<expr_res; i++)
			{
				char *substring_start = info->dest_file + ovector[2*i];
				int substring_length = ovector[2*i+1] - ovector[2*i];
				sprintf(buffer[i],"%.*s",substring_length,substring_start);
				//printf("%d : %.*s\n",i,substring_length,substring_start);
			}
			info->source_file = buffer[1];
			if(info->pcre_type == 1)
			{
				/** combind source_file **/
				strcat(info->source_file,"/");
				strcat(info->source_file,buffer[2]);
				strcat(info->source_file,".");
				strcat(info->source_file,buffer[6]);
				/** combind request_filename **/
				info->request_filename = buffer[2];
				strcat(info->request_filename,".");
				strcat(info->request_filename,buffer[6]);
			}
			else
			{
				/** combind source_file **/
				strcat(info->source_file,"/");
				strcat(info->source_file,buffer[2]);
				/** combind request_filename **/
				info->request_filename = buffer[2];
			}
			dirname(buffer[1],info->local_dir);
			info->dest_file = buffer[0];
			info->m_type = buffer[3];
			info->max_width = atoi(buffer[4]);
			info->max_height = atoi(buffer[5]);
			info->max_width = (info->max_width > 2000) ? 2000 : info->max_width;
			info->max_height = (info->max_height > 2000) ? 2000 : info->max_height;
			if(info->max_width <= 0 || info->max_height <=0 ){
                        	//如果图片小于等于0，则可以判断请求无效了
                        	pcre_free(expr);
				pcre_malloc = old_pcre_malloc;
				pcre_free = old_pcre_free;
                        	return -1;
                        }
			//printf("source_file:%s\n",info->source_file);
			if(file_exists(info->source_file) == -1)//原图不存在
			{
				download(conf);
			}
			if(file_exists(info->source_file) == 0)
			{
				pcre_state = calc_image_info(conf);
				pcre_free(expr);
				pcre_malloc = old_pcre_malloc;
				pcre_free = old_pcre_free;
				return pcre_state;
			}
		}
		pcre_free(expr);
		//恢复Nginx默认PCRE内存分配
		pcre_malloc = old_pcre_malloc;
		pcre_free = old_pcre_free;
		//END
	}
	return -1;
}
static int calc_image_info(void *conf)
{
	ngx_image_conf_t *info = conf;
	info->src_type = get_ext_header(info->source_file);//读取原图头部信息金星判断图片格式
	if( info->src_type > 0)
	{
        info->w_margin = 0;//设置默认图片不补白边
		info->src_im = NULL;
		info->dst_im = NULL;
        info->w_im = NULL;
		image_from(conf);//读取原图图片到GD对象
		if(info->src_im != NULL)
		{

			info->src_width = info->src_im->sx;
			info->src_height = info->src_im->sy;
			info->src_x = 0;
			info->src_y = 0;
            info->dst_x = 0;
            info->dst_y = 0;
			info->src_w = info->src_width;
			info->src_h = info->src_height;
			info->width = info->max_width;
			info->height = info->max_height;
			if(stricmp(info->m_type,"c") == 0 || stricmp(info->m_type,"m") == 0)
			{
				if((double)info->src_width/info->src_height > (double)info->max_width / info->max_height)
				{
					info->src_w=info->width * info->src_height / info->height;
					if(stricmp(info->m_type,"m") == 0)
					{
						info->src_x=(info->src_width-info->src_w)/2;
					}
					else
					{
						info->src_x=(info->src_width-info->src_w)*0.1;
					}
				}
				else
				{
					info->src_h=info->src_w * info->height / info->width;
					if(stricmp(info->m_type,"m") == 0)
					{
						info->src_y=(info->src_height-info->src_h)/2;
					}
					else
					{
						info->src_y=(info->src_height-info->src_h)*0.1;
					}
				}
			}
			else if(stricmp(info->m_type,"t") == 0)
			{
				if( (info->max_width >= info->src_width) || (info->max_height >= info->src_height ))
				{
					info->max_width = info->src_width;
					info->max_height = info->src_height;
					info->height = info->src_height;
					info->width = info->src_width;
				}
				else
				{
					if((double)info->src_width/info->src_height >= (double) info->max_width / info->max_height)
					{
						info->height=info->width * info->src_height/info->src_width;
						info->src_h=info->src_w * info->height / info->width;
						info->src_y=(info->src_height - info->src_h) / 2;
					}
					else
					{
						info->width=info->max_height * info->src_width / info->src_height;
						info->src_w=info->width * info->src_height / info->height;
						info->src_x=(info->src_width - info->src_w)/2;
					}
				}
			}
			else if(stricmp(info->m_type,"w") == 0)
			{
				info->w_margin = 1;
				if((double)info->src_width/info->src_height >= (double) info->max_width / info->max_height)
				{
					info->height=info->width * info->src_height/info->src_width;
					info->src_h=info->src_w * info->height / info->width;
					info->src_y=(info->src_height - info->src_h) / 2;
				}
				else
				{
					info->width=info->max_height * info->src_width / info->src_height;
					info->src_w=info->width * info->src_height / info->height;
					info->src_x=(info->src_width - info->src_w)/2;
				}
                info->dst_x = (float)((float)(info->max_width - info->width)/2);
                info->dst_y = (float)((float)(info->max_height - info->height)/2);
                		
			}
			else
			{
				return -1;
			}
			return 0;
		}
	}
	return -1;
}

static void check_image_type(void *conf)
{
	ngx_image_conf_t *info = conf;
	info->extension = get_ext(info->dest_file);
	if(stricmp(info->extension,"jpg") == 0 || stricmp(info->extension,"jpeg") == 0 || stricmp(info->extension,"jpe") == 0)
	{
		info->dest_type = NGX_IMAGE_JPEG;
	}
	else if(stricmp(info->extension,"png") == 0)
	{
		info->dest_type = NGX_IMAGE_PNG;
	}
	else if(stricmp(info->extension,"gif") == 0)
	{
		info->dest_type = NGX_IMAGE_GIF;
	}
	else
	{
		info->dest_type = NGX_IMAGE_NONE;
	}
}

static char * get_ext(char *filename)
{
	char *p = strrchr(filename, '.');
	if(p != NULL)
	{
		return p+1;
	}
	else
	{
		return filename;
	}
}

static int file_exists(char *filename)
{
	if(access(filename, 0) != -1)
	{
		return 0;
	}
	else
	{
		return -1;
	}
}

static int get_ext_header(char *filename)
{
	FILE *handle;
	handle = fopen(filename, "rb");
	if(!handle)
	{
		return NGX_IMAGE_NONE;
	}
	else
	{
		unsigned short filetype;//bmp 0x4D42
		if(fread(&filetype,sizeof(unsigned short),1,handle) != 1)
		{
			fclose(handle);
			return NGX_IMAGE_NONE;
		}
		fclose(handle);
		if( filetype == 0xD8FF)
		{
			return NGX_IMAGE_JPEG;
		}
		else if( filetype == 0x4947)
		{
			return NGX_IMAGE_GIF;
		}
		else if( filetype == 0x5089)
		{
			return NGX_IMAGE_PNG;
		}
		else
		{
			return NGX_IMAGE_NONE;
		}
	}
	return NGX_IMAGE_NONE;
}

static void write_img(void * conf)
{
	ngx_image_conf_t *info = conf;
	FILE * fp;
	if(info->img_data == NULL)
	{
		return;
	}
	fp = fopen(info->dest_file,"wb");
	if(fp)
	{
		fwrite(info->img_data,sizeof(char),info->img_size,fp);
		fclose(fp);
	}
    gdFree(info->img_data);
}




static int read_img(char **filename,int * size,void ** buffer)
{
	FILE *fp;
	struct stat stat_buffer;
	fp = fopen(*filename, "rb");
	if (fp)
	{
		fstat(fileno(fp),&stat_buffer);
		*buffer = malloc(stat_buffer.st_size);
		if(fread(*buffer,1,stat_buffer.st_size,fp))
		{
			*size = stat_buffer.st_size;
		}
		fclose(fp);
		return 0;
	}
	return -1;
}

static void water_image_from(void * conf)
{
	int size = 0;
	void * buffer;
	char * water_file;
	ngx_image_conf_t *info = conf;
	info->water_im = NULL;
	water_file = (char *)info->water_image.data;
	info->water_im_type = get_ext_header(water_file);
	if((read_img(&water_file,&size,&buffer)) == 0)
	{
		switch(info->water_im_type)
		{
		case NGX_IMAGE_GIF:
			info->water_im = gdImageCreateFromGifPtr(size,buffer);
			break;
		case NGX_IMAGE_JPEG:
			info->water_im = gdImageCreateFromJpegPtr(size,buffer);
			break;
		case NGX_IMAGE_PNG:
			info->water_im = gdImageCreateFromPngPtr(size,buffer);
			break;
		}
		free(buffer);
		return;
	}
}

static void image_from(void * conf)
{
	int size = 0;
	void * buffer;
	ngx_image_conf_t *info = conf;
	info->src_im = NULL;
	if((read_img(&info->source_file,&size,&buffer)) == 0)
	{
		switch(info->src_type)
		{
			case NGX_IMAGE_GIF:
				info->src_im = gdImageCreateFromGifPtr(size,buffer);
				break;
			case NGX_IMAGE_JPEG:
				info->src_im = gdImageCreateFromJpegPtr(size,buffer);
				break;
			case NGX_IMAGE_PNG:
				info->src_im = gdImageCreateFromPngPtr(size,buffer);
				break;
		}
		free(buffer);
		return;
	}
}

static int get_header(char *url)
{
	long result = 0;
	CURL *curl;
	CURLcode status;

	curl = curl_easy_init();

	memset(&status,0,sizeof(CURLcode));
	if(curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL,url);
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST,"GET");
		status = curl_easy_perform(curl);
		if(status != CURLE_OK)
		{
			curl_easy_cleanup(curl);
			return -1;
		}
		status = curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&result);
		if((status == CURLE_OK) && result == 200)
		{
			curl_easy_cleanup(curl);
			return 0;
		}
		curl_easy_cleanup(curl);
		return -1;
	}
	return -1;
}

static size_t curl_get_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
	int written = fwrite (ptr, size, nmemb, (FILE *) curl_handle);
	return written;
}

static void dirname(char *path,char *dirpath)
{
	char dirname[255];
	int len = 0;
	len=strlen(path);
    memset(dirname,0,sizeof(dirname));
    for (;len>0;len--){ //从最后一个元素开始找.直到找到第一个'/'
		if(path[len]=='/')
		{
			strncpy(dirname,path,len+1);
			dirname[len] = '\0';
			break;
		}
    }
    *&dirpath = strdup(dirname);
}

static int create_dir(char *dir)
{
	int i;
	int len;
	char dirname[256];
	strcpy(dirname,dir);
	len=strlen(dirname);
	if(dirname[len-1]!='/')
	{
		strcat(dirname,"/");
	}
	len=strlen(dirname);
	for(i=1; i<len; i++)
	{
		if(dirname[i]=='/')
		{
			dirname[i] = 0;
			if(access(dirname,0)!=0)
			{
				if(mkdir(dirname,0777)==-1)
				{
					return -1;
				}
			}
			dirname[i] = '/';
		}
	}
	return 0;
}

static void get_request_source(void * conf)
{
	ngx_image_conf_t *info = conf;
	char real_url[512];//真实URL
	strcpy(real_url,(char *)info->backend_server.data);
	strcat(real_url,info->request_dir);
	strcat(real_url,"/");
	strcat(real_url, info->request_filename);
	info->request_source = strdup(real_url);
}


static void download(void * conf)
{
	ngx_image_conf_t *info = conf;
	get_request_source(conf);//取得请求的URL的原始文件
	if (get_header(info->request_source) == 0)
	{
		CURL *curl;
		curl = curl_easy_init();
		create_dir(info->local_dir);//创建目录
		if((curl_handle = fopen(info->source_file, "wb")) == NULL)
		{
			curl_easy_cleanup (curl);
			fclose(curl_handle);
			curl_handle = NULL;
			return;
		}
		if(curl)
		{
			curl_easy_setopt(curl, CURLOPT_URL,info->request_source);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_get_data);
			curl_easy_perform(curl);
			curl_easy_cleanup(curl);
			fclose(curl_handle);
			curl_handle = NULL;
		}
	}
	free(info->request_source);
}


