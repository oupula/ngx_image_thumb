# Nginx Image Thumbnail Module


##### EMail:<oopul@msn.com>
##### Website:<http://www.oupula.org>


### Module Support Nginx And tengine

- This Nginx Image module is auto create / output picture thumbnail, support for text watermark and image watermark.
- Support for custom font, text size, transparency of the watermark, the watermark position... 


## How To Build SourceCode 

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

#### DownLoad Nginx Image Module，put it under the nginx source code directory
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
$ wget https://github.com/3078825/nginx-image/archive/master.zip
$ unzip master.zip
$ ./configure --add-module=./nginx-image-master
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

Or use specified location

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
image on/off  #turn on/off image thumbnail module,default off.

image_backend on/off  #Mirror service is turn on/off, visit the picture url. if picture file does not exist, will be automatically downloaded from the mirror server into local server , default off.

image_backend_server #Mirror server url.

image_output on/off  #If On will not create picture thumbnail file , default off.

image_jpeg_quality 75 #Create jpeg picture quality , default 75.

image_water on/off #open watermark function , default off.

image_water_type 0/1 #Type of watermark 0: Image watermark  1: text watermark.

image_water_min 300 300 #source picture small than width:300 Height:300 will not add watermark.

image_water_pos 0-9 #watermark position , 0 for a random positin, 1 for the top ranking left, 2 centered in the top , 3 for the top ranking right, 4 Central left, 5 Middle center, 6 Central habitat right, 7 bottom of the left, 8 centered at the bottom ,9 bottom of the right.

image_water_file #watermark filename (jpg/png/gif), absolute or relative path of the watermark image.

image_water_transparent #transparency of the watermark, default 20.

image_water_text #watermark text , default "Power By Vampire".

image_water_font_size #watermark text size, default 5.

image_water_font #use special font to make watermark , absolute or relative path of fontname. 

image_water_color #watermakr text font color , default #000000
```

## How To use

If your nginx bind `127.0.0.1`

Put a picture named `test.jpg` into nginx website root path


Visit `http://127.0.0.1/test.jpg!c300x200.jpg` nginx/tengine will auto  create/output   `test.jpg` **width:300 height:200** thumbnail picture

In **c** parameter type of thumbnail， **300** is thumbnail picture width size     **200** is thumbnail picture height size

Support create / output total of four different types of thumbnails.

Support JPEG / PNG / GIF {not animation}


**C** parameter is aspect ratio from  **10%**  at the height of the image start capturing images, and zoom (in / out) to the specified size（ **Picture thumbnail size equal the request size** ）
 
**M** Parameter is aspect ratio of center screenshot picture, and then zoom (in / out) to the specified size（ **Picture thumbnail size equal the request size** ）

**T** parameters is aspect ratio scaling / zoom (in / out) to the specified size（ **Picture thumbnail size maybe small than request size** )

**W** parameter is aspect ratio scaling / zoom (in / out) to the specified size, fill in the white background color（ **Picture thumbnail size equal the request size** ）


 
## Use example

- http://oopul.vicp.net/12.jpg!c300x300.jpg

- http://oopul.vicp.net/12.jpg!t300x300.jpg

- http://oopul.vicp.net/12.jpg!m300x300.jpg

- http://oopul.vicp.net/12.jpg!w300x300.jpg

- http://oopul.vicp.net/12.c300x300.jpg

- http://oopul.vicp.net/12.t300x300.jpg

- http://oopul.vicp.net/12.m300x300.jpg

- http://oopul.vicp.net/12.w300x300.jpg



