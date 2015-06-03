#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <math.h>

#define TIMEOUT_MS 1000 //timeout de um segundo

struct windowPos {
    int seqNumber;
    int AckRcvd;
    char *buffer;
    char *pkt;
};

int s,tam_buffer, tam_janela, tam_pkt;
int maxSeqNo;
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
        return 1; //janela cheia
    else return 0;
}

int windowEmpty (void) {
    if (windowIn == windowOut)
        return 1; //janela vazia
    else return 0;
}

void windowInsert (char *buffer, char* pkt, int seqNumber) {
    window[windowIn].buffer = (char*)malloc(tam_buffer);
    window[windowIn].pkt = (char*)malloc(tam_pkt);
    window[windowIn].seqNumber = seqNumber;
    window[windowIn].AckRcvd = 0;
    strcpy(window[windowIn].buffer, buffer);
    strcpy(window[windowIn].pkt, pkt);
    windowIn = (windowIn + 1)%tam_janela;
}

int removeAckds (void) {
    if (window[windowOut].AckRcvd) {
        free(window[windowOut].buffer);
        free(window[windowOut].pkt);
        windowOut = (windowOut + 1)%tam_janela;
        return 1;
    }
    else return 0;
}
void acknowledge (int seqNumber) { //número do ack representa que todos os pacotes antes desse chegaram na ordem
    int num, i;
    num = (seqNumber - window[windowOut].seqNumber)%maxSeqNo; //distância até o número do ack
    if (num<tam_janela) { //se ainda está na janela
        for (i=0; i<=num; i++) {
            window[windowOut + i].AckRcvd = 1;
        }
    }
}

void resend (int seqNumber) {
        int num;
        num = (seqNumber - window[windowOut].seqNumber)%tam_janela;
        sendto(s, (window+windowOut + num)->pkt, strlen((window+windowOut + num)->pkt),0, (struct sockaddr *)&cliaddr,sizeof(cliaddr));
}

void resendAll () {
    int i = windowOut;
    while (i!=windowIn) {
        sendto(s, window[i].pkt, strlen(window[i].pkt),0, (struct sockaddr *)&cliaddr,sizeof(cliaddr));
        i = (i+1)%tam_janela;
    }
}

void serialize (char *pkt, int seqNumber, unsigned long hash_no, char *buffer) {
    int i, a;
    a = sizeof(int);
    i = 0;
    while (a) { //serializa o inteiro
        a--;
        pkt[i] = seqNumber >> a*8;
        i++;
    }
    a = sizeof(unsigned long);
    while (a) { //serializa o unsigned long
        a--;
        pkt[i] = hash_no >> a*8;
        i++;
    }
    a = strlen(buffer);
    while (a) { //serializa a string
        a--;
        pkt[i] = buffer[a];
        i++;
    }
    pkt[i] = 0;
}

void deserialize (char *ack, int *ackNumber, char *buffer) {//"deserializa" o ack
    int i, a;
    i = 0;
    *ackNumber = 0;
    a = sizeof(int);
    while (a) {
        a--;
        *ackNumber += ack[i]*pow(8,a);
        i++;
    }
    a = strlen(ack) - sizeof(int);
    buffer[a+1] = 0;
    while(a) {
        a--;
        buffer[a] = ack[i];
        i++;
    }
}

int main(int argc, char**argv) {
    int ret, len, n, byte_count;
    struct addrinfo hints, *res;
    struct timeval tv0, tv1,tv;
    int seqNumber, ackNumber;
    unsigned long hash_no;
    char received[256], *pkt, aux[4];
    char *buffer;
    FILE *arquivo;
    pid_t child;

    if (argc != 4) {
        printf("Argumentos necessarios: porto_servidor, tam_buffer, tam_janela.\n");
        exit(1);
    }

    tam_buffer = atoi(argv[2]); //tamanho do buffer
    buffer = (char*)malloc(tam_buffer*sizeof(char)); //aloca o buffer
    tam_janela = atoi(argv[3]); //tamanho da janela
    window = (struct windowPos*)malloc(tam_janela*sizeof(struct windowPos)); //aloca janela
    maxSeqNo = 2*tam_janela; //há o dobro de números de sequência que elementos na janela
    tam_pkt = tam_buffer + sizeof(int) + sizeof(unsigned long);
    pkt = (char*)malloc(tam_pkt);
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
    //setando o timeout no socket
    tv.tv_sec = TIMEOUT_MS/1000;
    tv.tv_usec = 0;
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
            while (n = recvfrom(s,received,sizeof(received),0,(struct sockaddr *)&cliaddr, &len) < 0) {}
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
                        serialize(pkt,seqNumber,hash_no,buffer);
                        n = sendto(s, pkt, strlen(pkt),0, (struct sockaddr *)&cliaddr,sizeof(cliaddr));
                        windowInsert(buffer, pkt, seqNumber);
                        seqNumber = (seqNumber + 1)%maxSeqNo;
                        byte_count += n;
                    }
                    else break; //fim do arquivo
                }
                if(n = recvfrom(s,received,4+sizeof(int),0,(struct sockaddr *)&cliaddr, &len)<0) { //verifica se chegou algum ACK
                    resendAll(); //timeout
                }
                else {
                    received[n] = 0;
                    deserialize(received,&ackNumber,aux);
                    if (strcmp(aux, "ackn")) { //verifica se é ACK
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
