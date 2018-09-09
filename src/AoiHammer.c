#include "AoiHammer.h"
#include "StorageStruct.h"
#include "LinuxLog.h"
#include "TimeCalc.h"

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

#include <sys/param.h>

uint8_t cpuCount = 1;
static volatile uint8_t serverRunning = 1;

static const char *optString = "dl:p:f:h?";
struct globalArgs_t
{
    uint8_t isDaemon;
    char *listenAddr;
    uint16_t listenPort;
    char *persistenceFile;
} globalArgs;

/*
S 10 5 -7705324701675607121
S 10 5 -4210548948686533907
*/

int main(int argc, char *argv[])
{
    globalArgs.isDaemon = 0;
    globalArgs.listenAddr = "127.0.0.1";
    globalArgs.listenPort = 8023;
    globalArgs.persistenceFile = "hashs.export";
    int opt = getopt(argc, argv, optString);
    while (opt != -1)
    {
        switch (opt)
        {
        case 'd':
            globalArgs.isDaemon = 1;
            break;
        case 'l':
            globalArgs.listenAddr = optarg;
            break;
        case 'p':
            globalArgs.listenPort = atoi(optarg);
            break;
        case 'f':
            globalArgs.persistenceFile = optarg;
            break;
        case 'h':
        case '?':
        {
            printf("AoiHammer: Hamming distance search engine\r\n");
            printf("Usage: %s [OPTIONS]\r\n", argv[0]);
            printf("\t -d Running in daemon mode.\r\n");
            printf("\t -l [ADDR] Server listen address. Default: 127.0.0.1\r\n");
            printf("\t -p [PORT] Server listen port. Default: 8023\r\n");
            printf("\t -f [FILENAME] Persistence file path. Default: ./hashs.export\r\n");
            exit(EXIT_FAILURE);
        }
        break;
        }
        opt = getopt(argc, argv, optString);
    }
    if (globalArgs.isDaemon)
    {
        forkDaemon();
        tolog(&stdout);
        tolog(&stderr);
    }
    printf("AoiHammer SearchEngine v0.1%s", "\n");
    cpuCount = sysconf(_SC_NPROCESSORS_ONLN);
    loadPersistenceFile();
    socketServer(globalArgs.listenAddr, globalArgs.listenPort);
}

void savePersistenceFile(uint32_t id, uint64_t hash)
{
    FILE *fileDescriptor = fopen(globalArgs.persistenceFile, "a+");
    fseek(fileDescriptor, 0, SEEK_END);
    char buffer[512];
    sprintf(buffer, "{\"_id\":%u,\"hash\":{\"$numberLong\":\"%ld\"}}\n", id, hash);
    fputs(buffer, fileDescriptor);
    fclose(fileDescriptor);
}

void loadPersistenceFile()
{
    char buffer[512];
    time_t startTime = time(NULL);
    uint32_t counter = 0;
    uint32_t id;
    int64_t hash;
    char *prefix = "\r\x1B[K";
    if (globalArgs.isDaemon)
    {
        prefix = "";
    }
    FILE *fileDescriptor = fopen(globalArgs.persistenceFile, "r");
    if (fileDescriptor == NULL)
    {
        printf("Persistence data file: %s not found\n", globalArgs.persistenceFile);
        return;
    }
    printf("Start loading persistence data...%s", "\n");

    while (fgets(buffer, sizeof(buffer), fileDescriptor))
    {
        sscanf(buffer, "{\"_id\":%u,\"hash\":{\"$numberLong\":\"%ld\"}}", &id, &hash);
        if (counter & 131072)
        {
            printf("%sCurrent: [id: %u hash: %ld]", prefix, id, hash);
            fflush(stdout);
            counter = 0;
        }
        counter++;
        if (!insertData(id, hash))
        {
            printf("\nError on Load %u:%ld\n", id, hash);
        }
    }
    fclose(fileDescriptor);
    printf("%sCurrent: [id: %u hash: %ld]\n", prefix, id, hash);
    printf("Load finished in %lu seconds\n", time(NULL) - startTime);
}

void socketServer(const char *listenAddr, int listenPort)
{
    printf("Server listening on %s:%d\n", listenAddr, listenPort);
    int serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int optVal = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &optVal, sizeof(optVal));
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
    for (int descriptor = 0; descriptor < FD_SETSIZE; descriptor++)
    {
        if (FD_ISSET(descriptor, &socketDescriptorSet))
        {
            close(descriptor);
        }
    }
}

void socketMain(int descriptor)
{
    char *buffer;
    buffer = calloc(128, sizeof(char));
    int readLength = read(descriptor, buffer, 128 * sizeof(char));
    if (readLength <= 0)
    {
        return;
    }
    switch (buffer[0])
    {
    case 'A':
    {
        uint32_t insertId = 0;
        int64_t insertHash = 0;
        sscanf(buffer + 1, "%u %ld", &insertId, &insertHash);
        savePersistenceFile(insertId, insertHash);
        if (insertId)
        {
            if (insertData(insertId, insertHash))
            {
                sprintf(buffer, "ok\r\n");
            }
            else
            {
                sprintf(buffer, "error\r\n");
            }
        }
        else
        {
            sprintf(buffer, "error\r\n");
        }
    }
    break;
    case 'S':
    {
        int64_t queryHash;
        uint8_t maxDistance;
        uint8_t resultCount;
        sscanf(buffer + 1, "%hhu %hhu %ld", &maxDistance, &resultCount, &queryHash);
        SearchResult result[resultCount];
        timeCalc timer;
        startTimeCalc(&timer);
        startFastSearch(queryHash, 10, resultCount, result);
        if (!result[0].id)
        {
            startFullSearch(queryHash, maxDistance, resultCount, result);
        }
        double execTime = getTimeCalc(&timer);
        printf("Search: %ld | Execution time: %fs\n", queryHash, execTime);
        free(buffer);
        buffer = calloc(14 * resultCount + 1, sizeof(char));
        for (uint8_t i = 0; i < resultCount; i++)
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
        sprintf(buffer, "unknown\r\n");
    }
    }
    int writeLength = write(descriptor, buffer, strlen(buffer));
    if (writeLength <= 0)
    {
        printf("Write Socket Error\n");
    }
    free(buffer);
    buffer = NULL;
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
