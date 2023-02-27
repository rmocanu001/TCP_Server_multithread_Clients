#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/sendfile.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#define PORT 8888
#define MAX_CLIENTS 10




typedef struct file_details{
char name[50];
char recurent[10][80];
char path[100];
int open;
} file_details;

static file_details** list=NULL;
static int nr_files=1;

pthread_mutex_t mutex;

void rec_search(file_details*file){
    int fd=open(file->path,O_RDWR);
            if(fd<0){
            printf("[-]File doesn't exist on server...(%s)\n",file->name);
            }
        //to do
    close(fd);

}

void populateList(){
  DIR *d;
    struct dirent *dir;
    int index=0;
    nr_files=1;
    d = opendir("./files/");
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
          if(strcmp(dir->d_name,".")!=0&&strcmp(dir->d_name,"..")!=0){
          list=realloc(list,nr_files*sizeof(file_details*));
          list[index]=malloc(sizeof(file_details));
          strcpy(list[index]->name,dir->d_name);
          char fullPathN[300];
          strcpy(fullPathN, "./files/");
          strcat(fullPathN,dir->d_name);
          strcpy(list[index]->path,fullPathN);
          rec_search(list[index]);
          list[index]->open=0;
          index++;
          nr_files++;
        }
        }

        closedir(d);
    }
}


void actiune_LIST(int sockfd){
    char str[100];
    memset(str,0,100);
    for(int i=0; i<nr_files-1; i++){
    strcat(str,list[i]->name);
    strcat(str,"\0");
    }
    write(sockfd,str,strlen(str));
}

void actiune_GET(int sockfd, int fd){
    int bytesSend=0;
    int TotalbytesSend=0;
    while(bytesSend=sendfile(sockfd,fd,bytesSend+TotalbytesSend,4048)>0){
        TotalbytesSend+=bytesSend;
    }
    close(fd);
}

file_details* search_by_name(char*name){
    for(int i=0; i<nr_files-1; i++){
        if(strcmp(list[i]->name,name)==0){
            return list[i];
        }
    }
    return NULL;
}

void actiune_Update(int fd, int dim, int offset, char*caractere){
    lseek(fd,offset,SEEK_SET);
    write(fd,caractere,dim);
    close(fd);
}
void actiune_PUT(char*name, int dim, char*caractere){
    list=realloc(list,nr_files*sizeof(file_details*));
    list[nr_files-1]=malloc(sizeof(file_details));

    char fullPathN[300];
    strcpy(fullPathN, "./files/");
    strcat(fullPathN,name);
    strcpy(list[nr_files-1]->name,name);

    strcpy(list[nr_files-1]->path,fullPathN);
    rec_search(list[nr_files-1]);
    list[nr_files-1]->open=0;
    nr_files++;
    umask(0000);
    int fd=open(fullPathN, O_RDWR | O_CREAT , 0700);
            if(fd<0){
                printf("[-]File doesn't exist on server...(%s)\n",name);
                return;
            }
    actiune_Update(fd,dim,0,caractere);
    
}

void tratareCerere(char*str, int sockcl){
    printf("%s\n",str);
    char aux[300];
    strcpy(aux,str);
    char*opt=strtok(aux, "|");
    if(strcmp(opt,"LIST")==0){
        actiune_LIST(sockcl);
    }
    else if(strcmp(opt,"GET")==0){

        printf("%s\n",str);
        char s[300];
        strcpy(s,str);
        char*op=strtok(s, "|");
        char*filename=strtok(NULL,"|"); 
        file_details*file=search_by_name(filename);
        file->open++;
        int fd=open(file->path,O_RDONLY);
        if(fd<0){
            printf("[-]File doesn't exist on server...(%s)\n",filename );
            return;
        }
        actiune_GET(sockcl,fd);
        file->open--;
    }
    else if(strcmp(opt,"DELETE")==0){
                printf("%s\n",str);
        char s[300];
        strcpy(s,str);
        char*op=strtok(s, "|");
        char*filename=strtok(NULL,"|"); 
        file_details*file=search_by_name(filename);
        while(1){
            if(file->open==0){
                pthread_mutex_lock(&mutex);

                int ret=remove(file->path);
                if(ret==-1)
                    printf("ERROR");
                else printf("Succesfull remove\n");

                pthread_mutex_unlock(&mutex);

                populateList();
                break;

            }
        }

    }
    else if(strcmp(opt,"PUT")==0){
        char s[300];
        strcpy(s,str);
        char*op=strtok(s, "|");
        char*filename=strtok(NULL,"|");
        char*dim=strtok(NULL,"|");
        char*caractere=strtok(NULL,"|");
        pthread_mutex_lock(&mutex);
        actiune_PUT(filename,atoi(dim),caractere);
        pthread_mutex_unlock(&mutex);
        populateList();
    }
    else if(strcmp(opt,"UPDATE")==0){
        char s[300];
        strcpy(s,str);
        char*op=strtok(s, "|");
        char*filename=strtok(NULL,"|");
        char*start=strtok(NULL,"|");
        char*dim=strtok(NULL,"|");
        char*caractere=strtok(NULL,"|");

        file_details*file=search_by_name(filename);
        while(1){
        if(file->open==0){
            int fd=open(file->path,O_WRONLY);
            if(fd<0){
                printf("[-]File doesn't exist on server...(%s)\n",filename );
                return;
            }
            pthread_mutex_lock(&mutex);
            actiune_Update(fd,atoi(dim),atoi(start),caractere);
            pthread_mutex_unlock(&mutex);
            break;
        }
        }

    }
    else if(strcmp(opt, "SEARCH")==0){
        //to do
    }
    else {
        printf("Varianta incorecta\n");
    }
    //pthread_mutex_unlock(&mutex);

}


void *handle_client(void *arg) {
    static __thread int sockfd = 0;

    const int sk=*((int*)arg);
    sockfd=sk;

    char buffer[1024];
    memset(buffer,0,1024);
    int valread=1;
    
    while((valread = read(sockfd, buffer, 1024)) > 0) {
        printf("%d\n",valread);
        printf("buffer: %s",buffer);
        tratareCerere(buffer, sockfd);
        memset(buffer,0,1024);
    }

    close(sockfd);
    //free(arg);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    int sockfd, new_sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;

    populateList();

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    // Bind socket to IP and port
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(sockfd);
        exit(1);
    }

    // Listen for incoming connections
    if (listen(sockfd, MAX_CLIENTS) == -1) {
        perror("listen");
        close(sockfd);
        exit(1);
    }

    printf("Server is listening for incoming connections on port %d\n", PORT);

    pthread_mutex_init(&mutex,NULL);

    // Accept incoming connections
    while (1) {

        printf("AICI1\n");
        client_len = sizeof(client_addr);
        if ((new_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len)) == -1) {
            perror("accept");
            continue;
        }
        printf("AICI2\n");
        //printf("Connected: %s:%d, file descriptor: %d\n",inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), new_sockfd);

        pthread_t thread;
        printf("AICI3\n");
        if (pthread_create(&thread, NULL, handle_client, &new_sockfd) != 0) {
            perror("pthread_create");
            pthread_detach(thread);
            close(new_sockfd);
            continue;
        }
        //pthread_exit(NULL);
    }
    pthread_exit(NULL);
    pthread_mutex_destroy(&mutex);
    close(sockfd);
    return 0;
}