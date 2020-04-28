#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#define PORT "8000"
#define MAX_REQUEST_SIZE 20000
#define MAX_HEADER_LEN 20000

const char* HOME_DIR;

void create_server();
void* handle_request(void* request);
const char* parse_request(char* request);


typedef struct{
    char file_name[MAX_REQUEST_SIZE];
    int sockfd;
} Request;


int main(int argc, char *argv[]){
    if(argc < 2){
        printf("ERROR, no file path provided");
        puts("here");
        exit(1);
    }
    HOME_DIR = argv[1];
    create_server();
    return 0;
}

void create_server(){

    int sockfd, newfd;
    struct addrinfo hints, *servinfo, *cur;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    
    memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if(getaddrinfo(NULL,PORT,&hints,&servinfo) != 0){
        perror("Failed to get address info.");
        exit(1);
    }

    for(cur=servinfo;cur!=NULL;cur=cur->ai_next){
        /* Create a socket */
        if((sockfd = socket(cur->ai_family,cur->ai_socktype,cur->ai_protocol)) == -1){
            perror("Socket error");
            continue;
        }
        /* Bind the socket to an address (localhost) */
        if(bind(sockfd,cur->ai_addr,cur->ai_addrlen) == -1){
            perror("Bind error");
            close(sockfd);
            continue;
        }
        break;
    }

    freeaddrinfo(servinfo);
    
    /* Listen on that socket's address */
    if(listen(sockfd,10) == -1){
        perror("Listen Error.");
        exit(1);
    }

    printf("Awating Connection...\n");

    /* Loop forever accepting connections and handling requests */
    while(1){
        sin_size = sizeof(their_addr);
        if((newfd = accept(sockfd,(struct sockaddr*)&their_addr,&sin_size)) == -1){
            perror("Failed to accept.");
            exit(1);
        }else{
            puts("Accepted connection.");
        }

        pthread_t thread_id;
        const char* file_requested;
        char buffer[MAX_REQUEST_SIZE];

        if(recv(newfd,buffer,sizeof(buffer),0) > 0){
            /* If you've receaved something from the client, parse the request */
            file_requested = parse_request(buffer);
        }

        if(strlen(file_requested) > 0){
            /* Create a new request struct to hold the request information. This memory will be freed by the thread who uses it. */
            Request *request = (Request *)malloc(sizeof(request));

            strcpy(request->file_name,file_requested);
            request->sockfd = newfd;
            pthread_create(&thread_id,NULL,handle_request,request);

            memset(buffer,0,sizeof(buffer)); //Clear the buffer for the next request.
        }
        
    }
    close(sockfd);
    return;
}

/*
 * Take the client's HTTP request and attempt to serve the page.
 * The Input will always be a Reqeust struct containing the file name and the socket file descriptor.
 * Once you have sent a reply to the client close the socket and file resources and exit the thread.
 */
void* handle_request(void* request){
    Request *rq = (Request*) request;
    printf("Creating a new thread to handle request:%s\n",rq->file_name);
    
    char final_path[MAX_REQUEST_SIZE]; //Create a string which will hold the final file path.
    
    int request_fd; 
    int size = 0; //Size in bytes of the requested file.
    struct stat file_stat;

    strcpy(final_path,HOME_DIR); //The home directory will be the start of the final file path.
    strcat(final_path,rq->file_name);//Append the file name to the home directory creating the final file path.

    char header[MAX_HEADER_LEN]; //buffer which will contain the http header.
    request_fd = open(final_path,0);
    if(request_fd == -1){
        /* File not found */
        strcpy(header,"HTTP/1.0 404\n\n404 File not found");
    }
    else{
        /* File found */
        fstat(request_fd,&file_stat);
        size = (int)file_stat.st_size;
        snprintf(header,MAX_HEADER_LEN,"HTTP/1.0 200 OK\nContent-Length:%i\n\n",size);
    }

    /* Create a buffer large enough to hold the contents of the file. Copy the contents into to buffer and close the file*/
    char *file_buffer = malloc(size);
    read(request_fd,file_buffer,size); //If the file wasn't found the size will still be 0 and nothing needs to be read.
    close(request_fd);
    
    /* Create the entire reply which will contain the HTTP header + the contents of the requested file */
    char* reply = (char*)malloc(strlen(header) + size);
    strcpy(reply,header);
    memcpy(reply + strlen(header),file_buffer,size);

    send(rq->sockfd,reply,strlen(header) + size,0);

    /*Close the client socket fd and cleanup memory */
    close(rq->sockfd);
    free(file_buffer);
    free(reply);
    free(request);
    
    pthread_exit(0);
}
/*
 * Parse an http GET request and return the requested file name
 */
const char* parse_request(char* request){
    char* get = strstr(request,"GET"); //Search the request for the HTTP command "GET".
    if(get == NULL){
        return "\0";
    }
    else{
        puts(request); /* TODO: remove this. Just printing the entire http request to console for debugging purposes.*/

        const char* deli = " "; //Deliminator to seperate tokens will be a space.
        const char* file_request;
        strtok(request,deli); //We know the first token of out request will be "GET" so we can ignore it.
        file_request = strtok(NULL,deli); //We know that the second token in our request will be the file name.
        return file_request;
    }
}