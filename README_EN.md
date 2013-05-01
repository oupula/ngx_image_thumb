# Nginx Image Thumb Module


##### EMail:<3078825@qq.com>
##### Website:<http://www.oupula.org>


### Module Support Nginx And tengine

- 本nginx模块主要功能是对请求的图片进行缩略/水印处理，支持文字水印和图片水印。
- 支持自定义字体，文字大小，水印透明度，水印位置。
- 判断原图是否是否大于指定尺寸才处理。
....等等


## How To Builder SourceCode 

Module Require libcurl-dev  libgd2-dev  libpcre-dev Library Code

### For Debian / Ubuntu 
```bash
$ sudo apt-get install build-essential m4 autoconf automake make 
$ sudo apt-get install libgd2-noxpm-dev libcurl4-openssl-dev libpcre3-dev
```

### For CentOS /RedHat / Fedora
```bash
$ sudo yum install gcc-c++ zlib-devel make 
$ sudo yum install gd-devel pcre-devel libcurl-devel 
```

### Download Nginx / Tengine Source Code

#### DownLoad Nginx Image Module，并放置在nginx源代码的nginx_image_module目录下
#### Nginx or Tengine . You can choose one between these.

```bash
# Download Tengine
$ wget http://tengine.taobao.org/download/tengine-1.4.5.tar.gz
$ tar -zxvf tengine-1.4.5.tar.gz
$ cd tengine-1.4.5
```

```bash
# Download Nginx
$ wget http://nginx.org/download/nginx-1.4.0.tar.gz
$ tar -zxvf nginx-1.4.0.tar.gz
$ cd nginx-1.4.0
```

```bash
$ mkdir ./nginx_image_module
$ cd ./nginx_image_module
$ wget https://github.com/3078825/nginx-image/archive/master.zip
$ unzip master.zip
$ cd ..
$ ./configure --add-module=./nginx_image_module
$ make
$ sudo make install 
```

## How to Setting Nginx / Tengine

Open `nginx.conf` 

```bash
vim /etc/nginx/nginx.conf 
# if nginx.cof not exist , you can find it use `find / -name "nginx.conf`
```

In
```apache
location / {
   root html;
   # Add lines
   image on;
   image_output on;
}
```

Or

```apache
location /upload {
   root html; 
   # Add lines
   image on;
   image_output on;
}
```


## Configuration details：
```apache
image on/off 是否开启缩略图功能,默认关闭

image_backend on/off 是否开启镜像服务，当开启该功能时，请求目录不存在的图片（判断原图），将自动从镜像服务器地址下载原图

image_backend_server 镜像服务器地址

image_output on/off 是否不生成图片而直接处理后输出 默认off

image_jpeg_quality 75 生成JPEG图片的质量 默认值75

image_water on/off 是否开启水印功能

image_water_type 0/1 水印类型 0:图片水印 1:文字水印

image_water_min 300 300 图片宽度 300 高度 300 的情况才添加水印

image_water_pos 0-9 水印位置 默认值9 0为随机位置,1为顶端居左,2为顶端居中,3为顶端居右,4为中部居左,5为中部居中,6为中部居右,7为底端居左,8为底端居中,9为底端居右

image_water_file 水印文件(jpg/png/gif),绝对路径或者相对路径的水印图片

image_water_transparent 水印透明度,默认20

image_water_text 水印文字 "Power By Vampire"

image_water_font_size 水印大小 默认 5

image_water_font 文字水印字体文件路径

image_water_color 水印文字颜色,默认 #000000
```

## How To use

If your nginx bind `127.0.0.1`

In Nginx website root path exist `test.jpg` picture


Visit `http://127.0.0.1/test.jpg!c300x200.jpg` nginx/tengine will auto  create/output   `test.jpg` **300x200** thumb picture

In **c** is thumb type， **300** is thumb picture width size     **200** is thumb picture height size

Support Create / output **Four**type Thumb Picture。

Support JPEG / PNG / GIF {not animation}


**C** 参数按请求宽高比例从图片高度 **10%** 处开始截取图片，然后缩放/放大到指定尺寸（ **图片缩略图大小等于请求的宽高** ）
 
**M** 参数按请求宽高比例居中截图图片，然后缩放/放大到指定尺寸（ **图片缩略图大小等于请求的宽高** ）

**T** 参数按请求宽高比例按比例缩放/放大到指定尺寸（ **图片缩略图大小可能小于请求的宽高** )

**W** 参数按请求宽高比例缩放/放大到指定尺寸，空白处填充白色背景颜色（ **图片缩略图大小等于请求的宽高** ）


 
## Use example

- http://127.0.0.1/test.jpg!c300x300.jpg

- http://127.0.0.1/test.jpg!t300x300.jpg

- http://127.0.0.1/test.jpg!m300x300.jpg

- http://127.0.0.1/test.jpg!w300x300.jpg

- http://127.0.0.1/test.c300x300.jpg

- http://127.0.0.1/test.t300x300.jpg

- http://127.0.0.1/test.m300x300.jpg

- http://127.0.0.1/test.w300x300.jpg





