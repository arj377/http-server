# HTTP Server (C++)

## Overview

A multithreaded HTTP/1.1 server built from scratch in modern C++ using POSIX sockets. Supports persistent connections, concurrent request handling with a thread pool, static file serving, MIME type detection, and zero-copy file transfers using `sendfile()`.

## Features

- HTTP/1.1 request parsing
- GET request handling
- Static file serving
- Zero-copy file transfers using `sendfile()`
- MIME type detection
- HTTP Keep-Alive (persistent connections)
- Thread pool for concurrent clients
- Directory traversal protection (`..`)
- Error responses:
  - 400 Bad Request
  - 403 Forbidden
  - 404 Not Found
  - 405 Method Not Allowed
  - 500 Internal Server Error

## Technologies

- C++17
- POSIX sockets
- std::thread
- std::mutex
- Condition variables
- Thread pool
- sendfile()
- CMake

## Project Structure

```
http_server/
├── src/
│   └── main.cpp
├── static/
│   ├── index.html
│   ├── styles.css
│   ├── script.js
│   └── ...
├── ThreadPool.h
├── CMakeLists.txt
└── README.md
```

Open:

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

- Main thread accepts incoming TCP connections.
- Accepted sockets are submitted to a fixed-size thread pool.
- Worker threads parse HTTP requests.
- Static files are served directly from disk.
- Large files are transferred using zero-copy `sendfile()`.
- HTTP/1.1 Keep-Alive allows multiple requests per TCP connection.
