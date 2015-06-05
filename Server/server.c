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
    unsigned int seqNumber;
    int AckRcvd;
    char *buffer;
    char *pkt;
};

int s;
unsigned int tam_buffer, tam_janela, tam_pkt;
unsigned int maxSeqNo, windowIn, windowOut;
struct sockaddr_in6 cliaddr;
struct windowPos *window;

int mod(int a, int b) { //aritmética modular
    int r = a % b;
    return r<0 ? (r+b):r;
}

unsigned int checksum (char *str) {
    unsigned int i;
    unsigned int checkResult = 0;
    for (i=0; str[i]!='\0'; i++)
        checkResult += (unsigned int)str[i];
    return checkResult;
}

void windowInit(void) {
    windowIn = windowOut = 0;
}

int windowFull(void) {
    if (windowIn == mod((windowOut - 1 + tam_janela),tam_janela))
        return 1; //janela cheia
    else return 0;
}

int windowEmpty (void) {
    if (windowIn == windowOut)
        return 1; //janela vazia
    else return 0;
}

void windowInsert (char *buffer, char* pkt, unsigned int seqNumber) {
    window[windowIn].buffer = (char*)malloc(tam_buffer);
    window[windowIn].pkt = (char*)malloc(tam_pkt);
    window[windowIn].seqNumber = seqNumber;
    window[windowIn].AckRcvd = 0;
    strcpy(window[windowIn].buffer, buffer);
    strcpy(window[windowIn].pkt, pkt);
    windowIn = mod((windowIn + 1),tam_janela);
}

int removeAckds (void) {
    if (window[windowOut].AckRcvd) {
        free(window[windowOut].buffer);
        free(window[windowOut].pkt);
        windowOut = mod((windowOut + 1),tam_janela);
        return 1;
    }
    else return 0;
}
void acknowledge (unsigned int seqNumber) { //número do ack representa que todos os pacotes antes desse chegaram na ordem
    int num, i;
    num = mod((seqNumber - window[windowOut].seqNumber),maxSeqNo); //distância até o número do ack
    if (num<tam_janela) { //se ainda está na janela
        for (i=0; i<=num; i++) {
            window[windowOut + i].AckRcvd = 1;
        }
    }
}

void resend (unsigned int seqNumber) {
        int num;
        num = mod((seqNumber - window[windowOut].seqNumber),tam_janela);
        sendto(s, (window+windowOut + num)->pkt, strlen((window+windowOut + num)->pkt),0, (struct sockaddr *)&cliaddr,sizeof(cliaddr));
}

void resendAll () {
    unsigned int i = windowOut;
    while (i!=windowIn) {
        sendto(s, window[i].pkt, strlen(window[i].pkt),0, (struct sockaddr *)&cliaddr,sizeof(cliaddr));
        i = (i+1)%tam_janela;
    }
}

void serialize (unsigned char *pkt, unsigned int seqNumber, unsigned int checkResult, char *buffer) {
    unsigned int i, a;
    i = 0;
    a = sizeof(unsigned int);
    for (i=0; i<a; i++) { //serializa os dois inteiros
        pkt[i] = (seqNumber) >> 8*i;
        pkt[i]+=1;
        pkt[i+a] = (checkResult) >> 8*i;
        pkt[i+a]+=1;
    }
    i+=a;
    a = strlen(buffer);
    while (a) { //serializa a string
        a--;
        pkt[i] = buffer[i-8];
        i++;
    }
    pkt[i] = '\0';
}

void deserialize (unsigned char *ack, unsigned int *ackNumber, unsigned char *buffer, unsigned int n) {//"deserializa" o ack
    int i, a;
    i = 0;
    *ackNumber = 0;
    a = sizeof(int);
    for (i=0; i<a; i++) {
        *ackNumber += (ack[i]-1)*pow(256,i);
    }
    a = n - sizeof(int);
    buffer[a] = 0;
    while(a) {
        a--;
        buffer[i-4] = ack[i];
        i++;
    }
}

int main(int argc, char**argv) {
    int ret, len, conv, fim;
    unsigned int checkResult, n, byte_count;
    struct addrinfo hints, *res;
    struct timeval tv0, tv1,tv;
    unsigned int seqNumber, ackNumber;
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
    tam_pkt = tam_buffer + 2*sizeof(int);
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
            fim = 0; //marca o fim do arquivo
            while(!fim && !windowEmpty()) { //enquanto o arquivo não tiver chegado ao fim e todos os acks não tiverem sido recebidos
                while(!windowFull()) {
                    if(fread(buffer, 1, tam_buffer, arquivo)) {
                        checkResult = checksum(buffer);
                        /*conv = htonl(seqNumber);
                        strcpy(pkt, (char*)&conv);
                        conv = htonl(checkResult);
                        strcat(pkt, (char*)&conv);
                        strcat(pkt, buffer);*/
                        serialize(pkt,seqNumber,checkResult,buffer);
                        n = sendto(s, pkt, strlen(pkt),0, (struct sockaddr *)&cliaddr,sizeof(cliaddr)); //envia o pacote
                        while (n==-1) { //reenvia em caso de erro
                            n = sendto(s, pkt, strlen(pkt),0, (struct sockaddr *)&cliaddr,sizeof(cliaddr));
                        }
                        fprintf(stderr,"\nPacote %d enviado!\n", seqNumber);
                        windowInsert(buffer, pkt, seqNumber); //buferiza o que acabou de enviar
                        seqNumber = mod((seqNumber + 1),maxSeqNo);
                        byte_count += n;
                    }
                    else {
                        fim = 1;
                        break; //fim do arquivo
                    }
                }
                while(1) {
                    n = recvfrom(s,received,4+sizeof(int),0,(struct sockaddr *)&cliaddr, &len); //recebe potenciais acks
                    if(n<=0) { //erro
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {//timeout
                            puts("timeout");
                            resendAll(); //timeout
                            break;
                        }
                        else
                            perror("Erro no recebimento");
                        //caso não tenha dado timeout, continuar iterando
                    }
                    else {
                        received[n] = 0;
                        deserialize(received,&ackNumber,aux,n);
                        if (strcmp(aux, "ackn")) { //verifica se é ACK
                            acknowledge(ackNumber);//acknowledge
                            fprintf(stderr,"\nAck %d recebido!\n",ackNumber);
                            while(removeAckds()){}//remove pacotes confirmados da janela
                        }
                        break;
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
