#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string>
#include <mutex>
#include <thread>
#include <sstream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "ThreadPool.h"

#define PORT "8080"
#define BACKLOG 10

std::mutex logMutex;

void log(const std::string &message)
{
    std::lock_guard<std::mutex> lock(logMutex);
    std::cout << message << '\n';
}

// Convert based on if its IPv6 or IPv4
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

bool sendAll(int socket, const std::string &message)
{
    size_t total = message.size();
    size_t counter = 0;

    while (counter < total)
    {
        int charSent = send(socket, message.data() + counter, total - counter, 0);
        if (charSent <= 0)
        {
            return false;
        }
        else
        {
            counter += charSent;
        }
    }
    return true;
}

bool sendFileAll(int fd, int socket)
{
    struct stat buf;
    int stats = fstat(fd, &buf);
    if (stats == -1)
    {
        perror("stats");
        return false;
    }
    off_t total = buf.st_size;
    off_t sent = 0;

    while (sent < total)
    {
        off_t len = total - sent;

        if (sendfile(fd, socket, sent, &len, NULL, 0) == -1)
        {
            perror("sendfile");
            return false;
        }

        sent += len;
    }

    return true;
}

void handleClient(int socket)
{
    while (true)
    {
        bool shouldClose = false;
        bool isFileResponse = false;
        char buf[4096];
        int bytesRead = recv(socket, buf, sizeof(buf), 0);
        if (bytesRead == -1)
        {
            perror("recv");
            close(socket);
            return;
        }
        else if (bytesRead == 0)
        {
            log("Client disconnected.");
            close(socket);
            break; // Go back to accept() and wait for another client
        }
        std::string request(buf, bytesRead);
        std::istringstream parser(request);
        std::string method, path, version;
        parser >> method >> path >> version;
        std::string contentType;
        std::string line, header, connectionStatus;
        std::string openOrClose = "keep-alive";
        while (true)
        {
            getline(parser, line); // Check each subsequent header and overwrites line every iteration
            if (line.empty())
            {
                break;
            }
            if (line.size() < 11)
            {
                continue;
            }
            header = line.substr(0, 11);
            for (char &c : header)
            {
                c = std::tolower(c); // Convert to lowercase
            }
            if (header == "connection:")
            {
                connectionStatus = line.substr(13, 5); // Check specifically for close
                for (char &c : connectionStatus)
                {
                    c = std::tolower(c);
                }
                if (connectionStatus == "close")
                {
                    shouldClose = true;
                    openOrClose = "close";
                }
            }
        }

        log("Method: " + method);
        log("Path: " + path);
        log("Version: " + version);

        std::string body, status;
        char readBuf[4096];
        int fd = -1, n;

        if (path.find("..") != std::string::npos) // .. exists in the path
        {
            status = "403 Forbidden";
            body = "403 Forbidden";
            contentType = "text/plain";
        }
        else if (method.empty() || path.empty() || version.empty() ||
                 (version != "HTTP/1.1" && version != "HTTP/1.0")) // Valid HTTP request
        {
            status = "400 Bad Request"; // Invalid HTTP Request
            body = "400 Bad Request";
            contentType = "text/plain";
        }
        else if (method != "GET")
        {
            status = "405 Method Not Allowed"; // Method is not supported
            body = "405 Method Not Allowed";
            contentType = "text/plain";
        }
        else
        {
            if (path == "/") // Set to standard path
            {
                path = "/index.html";
            }

            std::string fileName = "../static" + path;
            fd = open(fileName.c_str(), O_RDONLY);
            if (fd == -1)
            {
                perror("open");
                contentType = "text/plain";
                body = "404 Not Found";
                status = "404 Not Found"; // Resource requested by client is not available
            }
            else
            {
                isFileResponse = true;
                status = "200 OK";
                size_t dotPlace = path.rfind("."); // Gets the last dot
                if (dotPlace == std::string::npos) // No dot place --> just set to octet-stream
                {
                    contentType = "application/octet-stream";
                }
                else
                {
                    std::string extension = path.substr(dotPlace);
                    if (extension == ".html")
                    {
                        contentType = "text/html";
                    }
                    else if (extension == ".css")
                    {
                        contentType = "text/css";
                    }
                    else if (extension == ".js")
                    {
                        contentType = "application/javascript";
                    }
                    else if (extension == ".jpg" || extension == ".jpeg")
                    {
                        contentType = "image/jpeg";
                    }
                    else if (extension == ".png")
                    {
                        contentType = "image/png";
                    }
                    else if (extension == ".gif")
                    {
                        contentType = "image/gif";
                    }
                    else if (extension == ".svg")
                    {
                        contentType = "image/svg+xml";
                    }
                    else if (extension == ".txt")
                    {
                        contentType = "text/plain";
                    }
                    else if (extension == ".json")
                    {
                        contentType = "application/json";
                    }
                    else if (extension == ".pdf")
                    {
                        contentType = "application/pdf";
                    }
                    else
                    {
                        contentType = "application/octet-stream";
                    }
                }
            }
        }
        int bodySize;
        if (!body.empty())
        {
            bodySize = body.size();
        }
        else
        {
            struct stat b;
            int stats = fstat(fd, &b);
            if (stats == -1)
            {
                perror("fstat");
                status = "500 Internal Server Error";
                body = "500 Internal Server Error";
                contentType = "text/plain";
                bodySize = body.size();
            }
            else
            {
                bodySize = b.st_size;
            }
        }

        std::string response =  "HTTP/1.1 " + status + "\r\n"
                                "Content-Type: " + contentType + "\r\n"
                                "Content-Length: " + std::to_string(bodySize) + "\r\n"
                                "Connection: " + openOrClose + "\r\n"
                                "\r\n" +
                                body;

        if (!sendAll(socket, response))
        {
            perror("send");
            break;
        }

        if (isFileResponse)
        {
            if (!sendFileAll(fd, socket))
            {
                perror("sendfile");
                status = "500 Internal Server Error";
                body = "500 Internal Server Error";
                contentType = "text/plain";
            }
        }
        close(fd);

        if (shouldClose == true)
        {
            break;
        }
    }
    close(socket);
}

int main()
{
    int status, sockfd, new_fd; // Status for getaddrinfo, and sockets for communication
    int yes = 1;                // For reuseaddr

    struct addrinfo hints, *p;
    struct addrinfo *servinfo;          // Holds the linked list from getaddrinfo
    struct sockaddr_storage their_addr; // Info about client's IP
    socklen_t addr_size;

    char ip[INET6_ADDRSTRLEN]; // Holds the string to present in the terminal

    memset(&hints, 0, sizeof(hints)); // Clear hints first
    hints.ai_family = AF_UNSPEC;      // IPv6 or IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // This computer

    status = getaddrinfo(NULL, PORT, &hints, &servinfo); // Load the linked list into servinfo

    if (status != 0)
    {
        fprintf(stderr, "gai-error: %s\n", gai_strerror(status));
        exit(1);
    }

    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        // Finds a valid socket
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1)
        {
            perror("server: socket");
            continue;
        }

        // Makes sure the socket can be reused
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
        {
            perror("setsockopt");
            close(sockfd);
            continue;
        }

        // Make sure port is open
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // Not needed anymore

    // Make sure a valid socket was found
    if (p == NULL)
    {
        fprintf(stderr, "Server failed to bind\n");
        exit(1);
    }

    // Begin listening for incoming connections
    if (listen(sockfd, BACKLOG) != 0)
    {
        perror("listen");
        exit(1);
    }

    std::cout << "HTTP Server: waiting for connections...\n";
    ThreadPool pool(12);

    while (true)
    {
        addr_size = sizeof(their_addr);
        // Accept any new connections
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        if (new_fd == -1)
        {
            perror("accept");
            continue;
        }

        // Convert client IP into readable form and print it
        if (inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), ip, sizeof(ip)) == NULL)
        {
            perror("inet_ntop");
            close(new_fd);
            continue;
        }

        log("Connected: " + std::string(ip));

        pool.enqueue([new_fd]
                     { handleClient(new_fd); }); // Creates a callable function instead of executing first
    }
    return 0;
}