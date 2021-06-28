# x11mirror-server


## Description

This is a HTTP server and it was created to demostrate abillities of
[x11mirror-client][] program only.


## Usage

```
% ./x11mirror-server -h
Usage: ./x11mirror-server [-p PORT] [OPTIONS]
Options:
  -h                        print this help
  -d                        daemonize and enables syslog logging
  -q                        be quiet, i.e. disables all logging
  -p PORT                   a port number to listen, default 8888
  -t CONNECTION_TIMEOUT     a connection timeout in seconds, default 15 sec.
  -I MEMORY_INCREMENT       increment to use for growing the read buffer, default 1024
  -D                        enable MHD debug, disabled by default
  -E                        enable epoll backend (Linux only)
  -F                        enable TCP Fast Open support (Linux only)
  -L DIR_PATH               a directory where files will be stored, default `.'
  -M MEMORY_LIMIT           max memory size per connection, default 131072
  -T THREADS_NUM            an amount of threads, default 1
```

## Dependencies

* C99 compiler
* GNU make
* `pkg-config`
* `ImageMagick` development files (must be built with X11 support) and all
  its dependencies (for instance, zlib)
* `libmicrohttpd` (recommended to build with 0.9.72+)


## Build

Tested only on GNU/Linux at the moment.
By default all dependencies will be found by `pkg-config`.

```
% make
```

If you have installed libmicrohttpd to non-standard location, then
you may tell where is libmicrohttpd has installed like that:

```
% export PKG_CONFIG_PATH=/path/to/lib/pkgconfig
% export LD_LIBRARY_PATH=/path/to/lib
% make
```


## Credits

I am grateful for provided support of
[libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/) developers.
Especially I thank to Evgeny Grin, which answered to many questions and
gave me helpful advices.


[x11mirror-client]: https://github.com/gh0stwizard/x11mirror-client
