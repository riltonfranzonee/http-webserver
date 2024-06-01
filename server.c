#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include <sys/stat.h>

#define MAX_PENDING_CONNECTIONS 10
#define PORT 8000
#define BUFFER_SIZE 4096

struct HttpRequest {
    char* method;
    char* url;
};

struct HttpRequest* parse_request(char* buffer) {
    static struct HttpRequest httprequest;
    httprequest.method = strtok(buffer, " ");
    if(httprequest.method == NULL) {
        httprequest.method = "";
        httprequest.url = "";
    } else {
        httprequest.url = strtok(NULL, " ") + 1;
    }
    return &httprequest;
}

int main() {
    int s = socket(PF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server;
    server.sin_family = PF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    socklen_t serverlen = (socklen_t) sizeof(server);

    bind(s, (struct sockaddr*) &server, serverlen);
    listen(s, MAX_PENDING_CONNECTIONS);
    
    printf("server listening on port %d\n", PORT);

    struct sockaddr_in client;
    socklen_t clientlen = (socklen_t) sizeof(client);

    while(1) {
        int clientsocket = accept(s, (struct sockaddr*) &client, &clientlen);
        printf("accepted connection from (%s:%d)\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
        
        char* buffer = calloc(BUFFER_SIZE, sizeof(char));
        recv(clientsocket, buffer, BUFFER_SIZE, 0);

        struct HttpRequest* req = parse_request(buffer);
        if(strcmp(req->method, "GET") != 0) {
            char* res = "HTTP/1.0 501 Method not implemented\r\n\r\n";
            send(clientsocket, res, strlen(res), 0);
            close(clientsocket);
            free(buffer);
            continue;
        }


        FILE* file = fopen(req->url, "r");
        if(file == NULL) {
            char* res = "HTTP/1.0 404 Not Found\r\nContent-Length: 18\r\n\r\n<h1>Not found</h1>";
            send(clientsocket, res, strlen(res), 0);
            close(clientsocket);
            free(buffer);
            continue;
        }

        struct stat file_status;
        stat(req->url, &file_status);

        char* res = calloc(BUFFER_SIZE, sizeof(char));
 
        // send HTTP status/headers packet separately
        sprintf(res, "HTTP/1.0 200 OK\r\nContent-Length: %lld\r\n\r\n", file_status.st_size); 
        send(clientsocket, res, strlen(res), 0);
        
        // send HTTP content packet(s)
        while(!feof(file)) {
          void* filebuffer = calloc(BUFFER_SIZE, sizeof(void));
          fread(filebuffer, BUFFER_SIZE, 1, file);
          send(clientsocket, filebuffer, BUFFER_SIZE, 0);
          free(filebuffer);
        }

        close(clientsocket); // close socket immediately after the response (HTTP 1.0)
        free(buffer);
        free(res);
    }
}
