#include "AoiHammer.h"
#include "StorageStruct.h"
#include "TimeCalc.h"

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/param.h>

uint8_t cpuCount = 1;
static volatile uint8_t serverRunning = 1;

void loadFromFile(const char *fileName);
void socketMain(int descriptor);
void socketServer(const char *listenAddr, int listenPort);
void forkDaemon();
void sigintHandler(int sig);

/*
S -7705324701675607121
F -7705324701675607121
S -4210548948686533907
F -4210548948686533907
*/

int main()
{
    // printf("Server Fork to daemon.\n");
    // forkDaemon();
    printf("AoiHammer SearchEngine v0.1\n");
    cpuCount = sysconf(_SC_NPROCESSORS_ONLN);
    loadFromFile("hashs.export");
    signal(SIGINT, sigintHandler);
    socketServer("127.0.0.1", 8023);
}

void loadFromFile(const char *fileName)
{
    FILE *fileDescriptor = fopen(fileName, "rb");
    if (fileDescriptor == NULL)
    {
        printf("Persistence data file: %s not found\n", fileName);
        return;
    }
    printf("Start loading persistence data...\n");
    char buffer[512];
    time_t startTime = time(NULL);
    uint16_t counter = 0;
    uint32_t id;
    int64_t hash;
    while (fgets(buffer, sizeof(buffer), fileDescriptor))
    {
        sscanf(buffer, "{\"_id\":%u,\"hash\":{\"$numberLong\":\"%ld\"}}", &id, &hash);
        if (!counter)
        {
            printf("\r\x1B[KCurrent: [id: %u hash: %ld]", id, hash);
            fflush(stdout);
        }
        if (!insertData(id, hash))
        {
            printf("\nError on Load %u:%ld\n", id, hash);
        }
        counter++;
    }
    printf("\r\x1B[KCurrent: [id: %u hash: %ld]", id, hash);
    printf("\nLoad finished in %lu seconds\n", time(NULL) - startTime);
}

void socketServer(const char *listenAddr, int listenPort)
{
    printf("Server listening on %s:%d\n", listenAddr, listenPort);
    int serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    fcntl(serverSocket, F_SETFL, SO_REUSEADDR | SO_REUSEPORT);
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(listenPort);
    serverAddress.sin_addr.s_addr = inet_addr(listenAddr);
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
    char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    int readLength = read(descriptor, buffer, sizeof(buffer) - 1);
    if (readLength <= 0)
    {
        return;
    }
    switch (buffer[0])
    {
    case 'A':
    {
        uint32_t insertId;
        int64_t insertHash;
        sscanf(buffer + 1, "%u %ld", &insertId, &insertHash);
        memset(buffer, 0, sizeof(buffer));
        if (insertId == 0)
        {
            sprintf(buffer, "error\r\n");
        }
        else if (insertData(insertId, insertHash))
        {
            sprintf(buffer, "ok\r\n");
        }
        else
        {
            sprintf(buffer, "error\r\n");
        }
    }
    break;
    case 'S':
    {
        timeCalc t;
        int64_t queryHash;
        sscanf(buffer + 1, "%ld", &queryHash);
        SearchResult result[10];
        startTimeCalc(&t);
        startFastSearch(queryHash, 10, 10, result);
        double execTime = getTimeCalc(&t);
        printf("Execution time: %fs\n", execTime);
        memset(buffer, 0, sizeof(buffer));
        for (uint8_t i = 0; i < 10; i++)
        {
            sprintf(buffer, "%s%u:%u,", buffer, result[i].id, result[i].distance);
        }
        buffer[strlen(buffer) - 1] = 0;
        sprintf(buffer, "%s\r\n", buffer);
    }
    break;
    case 'F':
    {
        timeCalc t;
        int64_t queryHash;
        sscanf(buffer + 1, "%ld", &queryHash);
        SearchResult result[10];
        startTimeCalc(&t);
        startFullSearch(queryHash, 10, 10, result);
        double execTime = getTimeCalc(&t);
        printf("Execution time: %fs\n", execTime);
        memset(buffer, 0, sizeof(buffer));
        for (uint8_t i = 0; i < 10; i++)
        {
            sprintf(buffer, "%s%u:%u,", buffer, result[i].id, result[i].distance);
        }
        buffer[strlen(buffer) - 1] = 0;
        sprintf(buffer, "%s\r\n", buffer);
    }
    break;
    case 'Q':
    {
        serverRunning = 0;
        sprintf(buffer, "shutdown\r\n");
    }
    break;
    default:
    {
        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "unknown\r\n");
    }
    }
    int writeLength = write(descriptor, buffer, strlen(buffer));
    if (writeLength <= 0)
    {
        printf("Write Socket Error\n");
    }
}

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
