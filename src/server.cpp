#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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

int main()
{
    int status, sockfd, new_fd;// Status for getaddrinfo, and sockets for communication
    struct addrinfo hints, *p;
    struct addrinfo *servinfo;          // Holds the linked list from getaddrinfo
    int yes = 1;                        // For reuseaddr
    struct sockaddr_storage their_addr; // Info about client's IP
    socklen_t addr_size;
    char s[INET6_ADDRSTRLEN]; // Holds the string to present in the terminal
    char buf[4096];
    

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
        if (inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof(s)) == NULL)
        {
            perror("inet_ntop");
            close(new_fd);
            continue;
        }
        log("Connected: " + std::string(s));

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
        std:: string input(buf, bytesRead);
        std::istringstream parser(input);
        std::string method, path, version;
        parser >> method >> path >> version;

        log("Method: " + method);
        log("Path: " + path);
        log("Version: " + version);


        close(new_fd);
    }
    return 0;
}