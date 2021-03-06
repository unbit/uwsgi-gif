uwsgi-gif
=========

uWSGI plugin for dynamic generation of simple gif images

Installation
============

The plugin is 2.0 friendly:

```sh
uwsgi --build-plugin https://github.com/unbit/uwsgi-gif
```

The procedure will generate the 'gif' plugin (gif_plugin.so) that you can put where you want (by default uWSGI searches for plugins in the current directory but you can load them specifying the absolute path)

Usage
=====

The plugin adds a 'gif' routing action, that by default generates a 1 pixel black gif:

 
```ini
[uwsgi]
plugin = gif
http-socket = :9090
; generate a 1 pixel black gif when asking for "/foo.gif"
route = ^/foo\.gif$ gif:
```

The 'gif' action takes a keyval list of parameters:

* width or w -> the width of the image
* height or h -> the height of the image
* red or r -> the red component of the image (0-255)
* green or g -> the green component of the image (0-255)
* blue or blu or b -> the blue component of the image (0-255)

So, if you want to generate a 80x80 red gif you can do:

```ini
[uwsgi]
plugin = gif
http-socket = :9090
; generate a 1 pixel black gif when asking for "/foo.gif"
route = ^/foo\.gif$ gif:width=80,height=80,red=255
```

or you can dynamically set colors:

```ini
[uwsgi]
plugin = gif
http-socket = :9090
route = ^/foo_(\d+)_(\d+)_(\d+)\.gif$ gif:width=80,height=80,red=$1,green=$2,blue=$3
```

(asking for ``/foo_255_255_0.gif`` will result in a yellow box)

Or you can use the QUERY_STRING

```ini
[uwsgi]
plugin = gif
http-socket = :9090
route = ^/foo\.gif$ gif:width=80,height=80,red=${qs[r]},green=${qs[g]},blue=${qs[b]}
```
(asking for ``/foo.gif?r=128&g=128&b=128`` will result in a gray box)

Notes
=====

The gif generation is really fast and consumes about 300 bytes of memory as the compression way used is pretty dumb. (it can compress about 50% of the pixels resetting the LZW dictionary every two of them). If you care bandwidth do not use the plugin for generation of really big images (or implement full LZW encoding and send a pull request for it ;)
