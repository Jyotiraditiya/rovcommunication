#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

int main()
{
    // Create UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0)
    {
        perror("Socket creation failed");
        return 1;
    }

    // ROV address
    sockaddr_in rovAddress{};

    rovAddress.sin_family = AF_INET;
    rovAddress.sin_port = htons(5000);
    rovAddress.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Bind socket
    if (bind(sockfd,
             reinterpret_cast<sockaddr*>(&rovAddress),
             sizeof(rovAddress)) < 0)
    {
        perror("Bind failed");
        close(sockfd);
        return 1;
    }

    std::cout << "ROV receiver listening on 127.0.0.1:5000\n";

    char buffer[2048];

    while (true)
    {
        sockaddr_in surfaceAddress{};
        socklen_t surfaceLength = sizeof(surfaceAddress);

        ssize_t bytesReceived = recvfrom(
            sockfd,
            buffer,
            sizeof(buffer),
            0,
            reinterpret_cast<sockaddr*>(&surfaceAddress),
            &surfaceLength
        );

        if (bytesReceived < 0)
        {
            perror("recvfrom failed");
            continue;
        }

        // Echo packet back to Surface
        ssize_t bytesSent = sendto(
            sockfd,
            buffer,
            bytesReceived,
            0,
            reinterpret_cast<sockaddr*>(&surfaceAddress),
            surfaceLength
        );

        if (bytesSent < 0)
        {
            perror("sendto failed");
        }
    }

    close(sockfd);

    return 0;
}