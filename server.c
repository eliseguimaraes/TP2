#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>

int main(int argc, char**argv) {
    int s, ret, len, n, conn, tam_buffer, byte_count;
    struct sockaddr_in6 cliaddr;
    struct addrinfo hints, *res;
    struct timeval tv0, tv1;
    char received[256], buffer[256];
    FILE *arquivo;
    pid_t child;

    if (argc != 4) {
        printf("Argumentos necessarios: porto_servidor, tam_buffer, tam_janela.\n");
        exit(1);
    }

    //criação do socket UDP

    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = 0;

    ret = getaddrinfo(NULL,argv[1],&hints,&res);
    if(ret) {
        printf("Endereço inválido.");
        exit(1);
    }

    s=socket(res->ai_family,SOCK_STREAM,0);
    puts("socket criado.\n");

    bind(s,res->ai_addr,res->ai_addrlen);
    puts("bind\n");

    //aguarda conexão em abertura passiva
    if (listen(s,10) == -1) {
        printf("\nErro ao aguardar conexão: %s", strerror(errno));
        exit(1);
    }

    while (1) {
        len=sizeof(cliaddr);
        conn = accept(s,(struct sockaddr *)&cliaddr,&len);

        if(conn == -1) {
            perror("Erro ao aceitar conexão.\n");
            exit(1);
        }
        puts("Conectado ao cliente.\n");

        if ((child = fork())==0) { //caso seja processo filho
            close(s);
            gettimeofday(&tv0,0);//inicia a contagem de tempo
            n = recv(conn,received,sizeof(received),0);
            received[n] = 0;
            arquivo = fopen(received, "r");
            if (arquivo == NULL) {
                printf("Erro ao abrir o arquivo.");
                exit(1);
            }
            tam_buffer = argv[2]; //tamanho do buffer
            byte_count = 0; //inicia contagem de bytes enviados
            while(fread(buffer, 1, tam_buffer, arquivo)) {
                 n = send(conn, buffer, strlen(buffer),0);
                 byte_count += n;
            }
            fclose(arquivo);
            gettimeofday(&tv1,0);//encera a contagem de tempo
            long total = (tv1.tv_sec - tv0.tv_sec)*1000000 + tv1.tv_usec - tv0.tv_usec; //tempo decorrido, em microssegundos
            printf("\nDesempenho: \nBytes enviados: %d \nTempo decorrido (microssegundos): %ld\nThroughput: %f bytes/segundo\n", byte_count, total, (float)byte_count*1000000/total);
        }
    }

    return 0;
}
