# HTTP Server (C++)

## Overview

A multithreaded HTTP/1.1 server built from scratch in modern C++ using POSIX sockets. The server supports persistent connections, concurrent request handling with a fixed-size thread pool, static file serving, MIME type detection, and zero-copy file transfers using `sendfile()`.

## Features

- HTTP/1.1 request parsing
- GET request handling
- Static file serving
- Zero-copy file transfers using `sendfile()`
- MIME type detection
- HTTP Keep-Alive (persistent connections)
- Fixed-size thread pool (12 worker threads)
- Directory traversal protection (`..`)
- HTTP status codes:
  - 200 OK
  - 400 Bad Request
  - 403 Forbidden
  - 404 Not Found
  - 405 Method Not Allowed
  - 500 Internal Server Error

## Technologies

- C++17
- POSIX sockets
- `std::thread`
- `std::mutex`
- Condition variables
- Thread pool
- `sendfile()`
- CMake

## Project Structure

```text
http_server/
├── src/
│   └── server.cpp
├── static/
│   ├── index.html
│   ├── styles.css
│   ├── script.js
│   └── ...
├── ThreadPool.h
├── CMakeLists.txt
└── README.md
```

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Running

```bash
./server
```

Open your browser:

```
http://localhost:8080
```

## Example Requests

```bash
curl -v http://localhost:8080/
curl -v http://localhost:8080/index.html
curl -v http://localhost:8080/styles.css
curl -v http://localhost:8080/doesnotexist
curl -v -X POST http://localhost:8080/
```

## Architecture

- A single main thread accepts incoming TCP connections.
- Accepted sockets are submitted to a fixed-size thread pool.
- Worker threads parse HTTP requests and generate responses.
- Static files are served directly from disk.
- Large files are transferred using zero-copy `sendfile()`.
- HTTP/1.1 Keep-Alive allows multiple requests to reuse the same TCP connection.

## Performance

Performance was evaluated locally using `wrk` with persistent HTTP/1.1 connections.

| Concurrent Connections | Requests/sec | Average Latency | p99 Latency |
|-----------------------:|-------------:|----------------:|------------:|
| 250                    | ~65,153      | 179 us          | 232 us      |
| 500                    | ~65,193      | 179 us          | 229 us      |
| 1000                   | ~64,822      | 180 us          | 231 us      |

The server sustained approximately **65,000 requests per second** while maintaining **sub-millisecond latency** under workloads of up to **1000 concurrent connections** using a 12-thread worker pool and zero-copy static file serving via `sendfile()`.
