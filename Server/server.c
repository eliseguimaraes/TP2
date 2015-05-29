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

struct windowPos {
    int seqNumber;
    int AckRcvd;
    char *buffer;
};

void windowInit(int *windowIn, int *windowOut) {
    *windowIn = *windowOut = 0;
}

int windowFull(int tam_janela, int windowIn, int windowOut) {
    if (windowIn == ((windowOut - 1 + tam_janela)%tam_janela))
        return = 1; //janela cheia
    else return 0;
}

int windowEmpty (int windowIn, int windowOut) {
    if (windowIn == windowOut)
        return 1; //janela vazia
    else return 0;
}

void windowInsert (struct windowPos* window, struct windowPos newBuffer, int tam_janela, int *windowIn, int *windowOut) {
    window[*windowIn].seqNumber = newBuffer.seqNumber;
    window[*windowIn].AckRcvd = 0;
    strcpy(window[*windowIn].buffer, newBuffer, buffer);
    *windowIn = (*windowIn + 1)%tam_janela;
}

int removeAckds (struct windowPos *window, int *windowOut, int tam_janela) {
    if (window[*windowOut].AckRcvd) {
        *windowOut = (*windowOut + 1)%tam_janela;
        return 1;
    }
    else return 0;
}

void acknowledge (struct windowPos *window, int windowOut, int seqNumber, int tam_janela) {
    int num;
    //if (seqNumber < window[windowOut].seqNumber) seqNumber+=2*tam_janela;
    num = seqNumber - window[windowOut].seqNumber;
    window[windowOut + num].AckRcvd = 1;
}


int main(int argc, char**argv) {
    int s, ret, len, n, tam_buffer, byte_count, tam_janela, maxSeqNo;
    struct sockaddr_in6 cliaddr;
    struct addrinfo hints, *res;
    struct timeval tv0, tv1;
    char received[256];
    char *buffer;
    int windowIn, windowOut;
    struct windowPos *window;
    FILE *arquivo;
    pid_t child;

    if (argc != 4) {
        printf("Argumentos necessarios: porto_servidor, tam_buffer, tam_janela.\n");
        exit(1);
    }

    tam_buffer = argv[2]; //tamanho do buffer
    buffer = (char*)malloc(tam_buffer*sizeof(char)); //aloca o buffer
    tam_janela = argv[3]; //tamanho da janela
    window = (struct windowPos*)malloc(tam_janela*sizeof(struct windowPos)); //aloca janela
    maxSeqNo = 2*tam_janela - 1; //hã o dobro de números de sequência que elementos na janela

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

    s=socket(res->ai_family,SOCK_DGRAM,0);
    puts("socket criado.\n");

    bind(s,res->ai_addr,res->ai_addrlen);
    puts("bind\n");


    while (1) {
        len=sizeof(cliaddr);


        //if ((child = fork())==0) { //caso seja processo filho
         //   close(s);
            gettimeofday(&tv0,0);//inicia a contagem de tempo
            n = recv(conn,received,sizeof(received),0);
            received[n] = 0;
            arquivo = fopen(received, "r");
            if (arquivo == NULL) {
                printf("Erro ao abrir o arquivo.");
                exit(1);
            }
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
