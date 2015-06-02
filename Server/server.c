#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>

#define TIMEOUT_MS 1000 //timeout de um segundo

struct windowPos {
    long int seqNumber;
    int AckRcvd;
    char *buffer;
    char *pkg;
};

int s,tam_buffer, tam_janela, tam_pkg;
long int maxSeqNo;
struct sockaddr_in6 cliaddr;
int windowIn, windowOut;
struct windowPos *window;

unsigned long hash(unsigned char *str) {
//gera um hash para uma string, para detecção de erro
//djb2
//retrieved from: http://www.cse.yorku.ca/~oz/hash.html
    unsigned long hash = 5381;
    int c;
    while (c = *str++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    return hash;
}


void windowInit(void) {
    windowIn = windowOut = 0;
}

int windowFull(void) {
    if (windowIn == ((windowOut - 1 + tam_janela)%tam_janela))
        return = 1; //janela cheia
    else return 0;
}

int windowEmpty (void) {
    if (windowIn == windowOut)
        return 1; //janela vazia
    else return 0;
}

void windowInsert (char *buffer, char* pkg, long int seqNumber) {
    window[windowIn].buffer = (char*)malloc(tam_buffer);
    window[windowIn].pkg = (char*)malloc(tam_pkg);
    window[windowIn].seqNumber = seqNumber;
    window[windowIn].AckRcvd = 0;
    strcpy(window[windowIn].bufferhttps://www.dropbox.com/sh/36qj54z7bf50vxu/AADPD6i75WG6dq0Kzunpuej7a?dl=0&preview=2011-02-03+02.01.47.jpg, buffer);
    strcpy(window[windowIn].pkg, pkg);
    windowIn = (windowIn + 1)%tam_janela;
}

int removeAckds (void) {
    if (window[windowOut].AckRcvd) {
        free(window[windowOut].buffer);
        windowOut = (windowOut + 1)%tam_janela;
        return 1;
    }
    else return 0;
}
void acknowledge (long int seqNumber) { //ack received means all packages before that arrived in order
    int num, i;
    num = (seqNumber - window[windowOut].seqNumber)%maxSeqNo;
    for (i=0; i<=num; i++) {
        window[windowOut + i].AckRcvd = 1;
    }
}

void resend (long int seqNumber) {
        int num;
        if (seqNumber < window[windowOut].seqNumber) seqNumber+=2*maxSeqNo;
        num = seqNumber - window[windowOut].seqNumber;
        sendto(s, window[windowOut + num].pkg, strlen(window[windowOut + num].pkg),0, (struct sockaddr *)cliaddr,sizeof(cliaddr));
}

void resendAll () {
    int i = windowOut;
    while (i!=windowIn) {
        sendto(s, window[i].pkg, strlen(window[i].pkg),0, (struct sockaddr *)cliaddr,sizeof(cliaddr));
        i = (i+1)%tam_janela;
    }
}


int main(int argc, char**argv) {
    int ret, len, n, byte_count;
    struct addrinfo hints, *res;
    struct timeval tv0, tv1,tv;
    long int seqNumber, ackNumber;
    unsigned long hash_no;
    char received[256], sent[256];
    char *buffer;
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
    maxSeqNo = 2*tam_janela; //há o dobro de números de sequência que elementos na janela
    windowInit(); //inicializa a janela

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

    tv.tv_sec = 0;
    tv.tv_usec = TIMEOUT_MS*1000; //setando o timeout
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv,sizeof(tv))<0) {
        perror("Error");
    }

    bind(s,res->ai_addr,res->ai_addrlen);
    puts("bind\n");


    while (1) {
        len=sizeof(cliaddr);
        //if ((child = fork())==0) { //caso seja processo filho
         //   close(s);
            gettimeofday(&tv0,0);//inicia a contagem de tempo
            n = recvfrom(s,received,sizeof(received),0,(struct sockaddr *)&cliaddr, &len);
            received[n] = 0;
            puts(received);
            arquivo = fopen(received, "r");
            if (arquivo == NULL) {
                printf("Erro ao abrir o arquivo.");
                exit(1);
            }
            byte_count = 0; //inicia contagem de bytes enviados
            seqNumber = 0;
            while(1) {
                if(!windowFull()) {
                    if(fread(buffer, 1, tam_buffer+1, arquivo)) {
                        hash_no = hash(buffer);
                        strcpy(sent, htonl(hash_no)); //detecção de erro: 32 bits
                        strcat(sent, htonl(seqNumber)); //número de sequência: 32 bits
                        strcat(sent, buffer); //dados: tamanho do buffer
                        n = sendto(s, sent, strlen(sent),0, (struct sockaddr *)&cliaddr,sizeof(cliaddr));
                        windowInsert(buffer, sent, seqNumber);
                        seqNumber = (seqNumber + 1)%maxSeqNo;
                        byte_count += n;
                    }
                    else break; //fim do arquivo
                }
                if(n = recvfrom(s,received,2*sizeof(unsigned long),0,(struct sockaddr *)&cliaddr, &len)<0) { //verifica se chegou algum ACK
                    resendAll(); //timeout
                }
                else {
                    received[n] = 0;
                    if (strncmp(received, "ackn",4)) { //verifica ACK (32 primeiros algarismos do PI para correção)
                        ackNumber = ntohl((received+4),0); //converte o número de sequência enviado no ack para número
                        acknowledge(ackNumber);//acknowledge
                        while(removeAckds()){}//remove pacotes confirmados da janela
                        resendAll(); //reenvia todos os que não foram confirmados
                    }
                }
            }

            fclose(arquivo);
            gettimeofday(&tv1,0);//encera a contagem de tempo
            long total = (tv1.tv_sec - tv0.tv_sec)*1000000 + tv1.tv_usec - tv0.tv_usec; //tempo decorrido, em microssegundos
            printf("\nDesempenho: \nBytes enviados: %d \nTempo decorrido (microssegundos): %ld\nThroughput: %f bytes/segundo\n", byte_count, total, (float)byte_count*1000000/total);
     //   }
    }

    return 0;
}
