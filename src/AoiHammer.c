#include "AoiHammer.h"
#include "StorageStruct.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/param.h>

static volatile uint8_t serverRunning = 1;

void forkDaemon()
{
    int pid;
    pid = fork();
    if (pid)
    {
        exit(0);
    }
    else
    {
        if (pid < 0)
        {
            exit(1);
        }
    }
    setsid();
    pid = fork();
    if (pid)
    {
        exit(0);
    }
    else
    {
        if (pid < 0)
        {
            exit(1);
        }
    }
    for (uint32_t i = 0; i < NOFILE; ++i)
    {
        close(i);
    }
}

void sigintHandler(int sig)
{
    signal(sig, SIG_IGN);
    serverRunning = 0;
    signal(SIGINT, sigintHandler);
}

int main(int argc, char **argv)
{
    // printf("Server Fork to daemon.\n");
    // forkDaemon();
    signal(SIGINT, sigintHandler);
    int serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    fcntl(serverSocket, F_SETFL, SO_REUSEADDR | SO_REUSEPORT);
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8023);
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1)
    {
        perror("Unable to bind port");
        exit(1);
    }
    if (listen(serverSocket, 20) == -1)
    {
        perror("Unable to listen");
        exit(1);
    }
    fd_set socketDescriptorSet;
    FD_ZERO(&socketDescriptorSet);
    FD_SET(serverSocket, &socketDescriptorSet);
    while (serverRunning)
    {
        fd_set socketDescriptorSetCopy = socketDescriptorSet;
        int selectCount = select(FD_SETSIZE, &socketDescriptorSetCopy, NULL, NULL, NULL);
        if (selectCount == -1)
        {
            perror("Unable to select");
            exit(1);
        }
        for (int descriptor = 0; descriptor < FD_SETSIZE; descriptor++)
        {
            if (FD_ISSET(descriptor, &socketDescriptorSetCopy))
            {
                if (descriptor == serverSocket)
                {
                    struct sockaddr_in clientAddress;
                    socklen_t sizeofClientSocketAddr = sizeof(clientAddress);
                    int clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddress, &sizeofClientSocketAddr);
                    FD_SET(clientSocket, &socketDescriptorSet);
                }
                else
                {
                    int readCount;
                    ioctl(descriptor, FIONREAD, &readCount);
                    if (readCount == 0)
                    {
                        close(descriptor);
                        FD_CLR(descriptor, &socketDescriptorSet);
                    }
                    else
                    {
                        socketMain(descriptor);
                    }
                }
            }
        }
    }
    for (int descriptor = FD_SETSIZE; descriptor >= 0; descriptor--)
    {
        if (FD_ISSET(descriptor, &socketDescriptorSet))
        {
            close(descriptor);
        }
    }
}

void socketMain(int descriptor)
{
    char buffer[128];
    memset(buffer, 0, sizeof(buffer));
    read(descriptor, buffer, sizeof(buffer) - 1);
    switch (buffer[0])
    {
    case 'A':
    {
        uint64_t insertId;
        int64_t insertHash;
        sscanf(buffer + 1, "%lu %ld", &insertId, &insertHash);
        memset(buffer, 0, sizeof(buffer));
        // printf("insertId: %lu insertHash: %ld\n", insertId, insertHash);
        if (insertData(insertId, insertHash))
        {
            sprintf(buffer, "ok\n");
        }
        else
        {
            sprintf(buffer, "error\n");
        }
    }
    break;
    case 'S':
    {
        int64_t queryHash;
        sscanf(buffer + 1, "%ld", &queryHash);
        pHashStore phash;
        phash.hash = queryHash;
        uint64_t resultId = startSearch(phash);
        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "%lu\n", resultId);
    }
    break;
    case 'Q':
    {
        serverRunning = 0;
        sprintf(buffer, "shutdown\n");
    }
    break;
    default:
    {
        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "unknown\n");
    }
    }
    write(descriptor, buffer, sizeof(buffer));
}
