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

        char buf[4096];
        int bytesRead = recv(new_fd, buf, sizeof(buf), 0);
        if (bytesRead == -1)
        {
            perror("recv");
            close(new_fd);
            continue;
        }
        else if (bytesRead == 0)
        {
            log("Client disconnected.");
            close(new_fd);
            continue; // Go back to accept() and wait for another client
        }
        std::string request(buf, bytesRead);
        std::istringstream parser(request);
        std::string method, path, version;
        parser >> method >> path >> version;
        std::string contentType;

        log("Method: " + method);
        log("Path: " + path);
        log("Version: " + version);

        std::string body, status;
        char readBuf[4096];
        int fd, n;

        if (path.find("..") != std::string::npos) // .. exists in the path
        {
            status = "403 Forbidden";
            body = "403 Forbidden";
            contentType = "text/plain";
        }
        else if (method.empty() || path.empty() || version.empty() ||
                (version != "HTTP/1.1" && version != "HTTP/1.0"))
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
            if (path == "/")
            {
                status = "200 OK";
                fd = open("../static/index.html", O_RDONLY); // ../ takes it back to the project root
                if (fd == -1)
                {
                    perror("open");
                    contentType = "text/plain";
                    body = "Failed to load /index.html";
                    status = "500 Internal Server Error"; // Sends an HTTP error back to the client
                }
                else
                {
                    contentType = "text/html";
                    while ((n = read(fd, readBuf, sizeof(readBuf))) > 0)
                    {
                        std::string rB(readBuf, n);
                        body += rB;
                    }

                    if (n == -1)
                    {
                        perror("read");
                        status = "500 Internal Server Error";
                        body = "500 Internal Server Error";
                        contentType = "text/plain";
                    }
                    close(fd);
                }
            }
            else 
            {
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
                    while ((n = read(fd, readBuf, sizeof(readBuf))) > 0)
                    {
                        std::string rB(readBuf, n);
                        body += rB;
                    }
                    if (n == -1)
                    {
                        perror("read");
                        status = "500 Internal Server Error";
                        body = "500 Internal Server Error";
                        contentType = "text/plain";
                    }
                    else
                    {
                        status = "200 OK";
                        size_t dotPlace = path.rfind("."); // Gets the last dot
                        if (dotPlace == std::string::npos)
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
                close(fd);
            }
        }
  
        std::string response = "HTTP/1.1 " + status + "\r\n"
                                "Content-Type: " + contentType + "\r\n"
                                "Content-Length: " + std::to_string(body.size()) + "\r\n"
                                "\r\n" +
                                body;

        if (!sendAll(new_fd, response)) {
            perror("send");
        }

        close(new_fd);
    }
    return 0;
}