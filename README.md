# Asynchronous Server

An implementation of an asynchronous web server written in C that
can handle simple HTTP GET requests. It uses Linux-specific functionalities
such `sendfile` as a zero-copy mechanism, `epoll` for I/O multiplexing and
creating an event loop and the Linux AIO API for asynchronous file operations.

# Installation

Make sure you have all the necessary dependencies installed:

```
$ sudo apt install libaio-dev
```

And afterwards you can run `make` to build the project.

# Usage

The name of the resulting executable is `server`.
By default, the server runs on localhost on port 8888,
but you can choose the port by modifying the `ASYNC_SERVER_PORT`
environment variable like in the following example:

```
$ ASYNC_SERVER_PORT=9999 ./server


# in another terminal

$ netstat -tulpn

Proto Recv-Q Send-Q Local Address           Foreign Address         State       PID/Program name
[...]
tcp        0      0 127.0.0.1:9999          0.0.0.0:*               LISTEN      30291/./server
[...]

```

The directory structure for the files that the server can handle
must be the following:
```
|
|-- static/ -> static files that do not need further processing
|-- dynamic/ -> templates/personalized content
|-- server -> the executable
|
```
In its current state, the only difference in the way the server
handles the two types of files is the mechanism it uses to transfer
data. More information on this topic can be found in the
[Implementation](#implementation) section.


# Implementation

The server was designed to be single-process and single-threaded and use I/O multiplexing
for increased efficiency. This was achieved using `epoll` and `eventfd` to create an event
loop for efficiently handling client requests without busy-waiting.

Where possible, files are sent using the `sendfile` syscall which
significantly reduces the overhead caused by mode switches. In cases where the transfer of
non-static files is desired, the server uses the Linux AIO API for asynchronous file
operations, receiving and sending data in 8kb chunks at a time. Wrappers for this
set of syscalls are provided by the `libaio-dev` package that needs to be installed
beforehand.

The HTTP parser used in this project is the one developed by Joyent and previously
used by [NodeJS](https://github.com/nodejs/http-parser). At the moment it is used solely for
extracting the path of the requested file from the request.

Aside from the parser and `libaio-dev`, no external libraries were used, most of the
functions used being either POSIX-compliant or Linux-specific.

More options and features are currently in development and will be added
as the project progresses.

# References

* [The Linux Programming Interface](https://man7.org/tlpi/)
-> Chapters 56 - 63

* [This tutorial](https://github.com/littledan/linux-aio)
 for using the Linux AIO API

* Various other examples of implementing web servers in C.