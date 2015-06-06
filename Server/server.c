#include "tp_socket.h"
#include "tp_socket.c"
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <math.h>

#define TIMEOUT_MS 0 //timeout milissegundos
#define TIMEOUT_S 1 //timeout segundos

struct windowPos {
    unsigned int seqNumber;
    int AckRcvd;
    char *buffer;
    char *pkt;
};

int s;
unsigned int tam_buffer, tam_janela, tam_pkt;
unsigned int maxSeqNo, windowIn, windowOut;
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
    if (windowIn == mod((windowOut - 1 + tam_janela),tam_janela)) {
        fprintf(stderr,"\nEntrada da janela: %d Saida da janela: %d\n",windowIn, windowOut);
        return 1; //janela cheia
    }
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
        window[windowOut].AckRcvd = 0;
        fprintf(stderr,"\nPacote %d confirmado e removido do buffer.\n",window[windowOut].seqNumber);
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

void resendAll (so_addr *cliaddr) {
    unsigned int i = windowOut;
    while (i!=windowIn) {
        tp_sendto(s, window[i].pkt, strlen(window[i].pkt), cliaddr);
        fprintf(stderr,"\nPacote %u reenviado\n",window[i].seqNumber);
        i = mod((i+1),tam_janela);
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

void deserializeAck (unsigned char *ack, unsigned int *ackNumber, unsigned char *buffer, unsigned int n) {//"deserializa" o ack
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
    int ret, len, fim,ack,flag, timeout,n,i;
    unsigned int checkResult, byte_count;
    struct addrinfo hints, *res;
    struct timeval tv0, tv1,tv;
    so_addr cliaddr;
    unsigned int seqNumber, ackNumber;
    char received[256], *pkt, aux[4], check = 0;
    char *buffer;
    FILE *arquivo;
    pid_t child;

    if(tp_init()) exit(1);

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

    s=tp_socket(atoi(argv[1]));
    puts("socket criado.\n");
    //setando o timeout no socket
    tv.tv_sec = TIMEOUT_S;
    tv.tv_usec = TIMEOUT_MS;
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv,sizeof(tv))<0)
        perror("Error");

    //while (1) {
        len=sizeof(cliaddr);
        //if ((child = fork())==0) { //caso seja processo filho
        //    close(s);
            gettimeofday(&tv0,0);//inicia a contagem de tempo
            n = tp_recvfrom(s,received,sizeof(received),&cliaddr);
            while (n < 0) {
                n = tp_recvfrom(s,received,sizeof(received),&cliaddr);
            }
            received[n] = 0;
            for (i=1; i<n; i++) check+=received[i];
            if (check!=received[0]) {
                puts("Erro no nome do arquivo");
                exit(1);
            }
            arquivo = fopen(received+1, "r");
            if (arquivo == NULL) {
                printf("Erro ao abrir o arquivo.");
                exit(1);
            }
            byte_count = 0; //inicia contagem de bytes enviados
            seqNumber = 0;
            fim = 0; //marca o fim do arquivo
            while(!fim || !windowEmpty()) { //enquanto o arquivo não tiver chegado ao fim e todos os acks não tiverem sido recebidos
                if (!fim) {
                    while(!windowFull()) {
                        if(n=fread(buffer, 1, tam_buffer, arquivo)) {
                            buffer[n]=0;
                            checkResult = checksum(buffer);
                            serialize(pkt,seqNumber,checkResult,buffer);
                            n = tp_sendto(s, pkt, 8+strlen(buffer),&cliaddr); //envia o pacote
                            while (n<8) { //reenvia em caso de erro
                                n = tp_sendto(s, pkt, 8+strlen(buffer),&cliaddr);
                            }
                            windowInsert(buffer, pkt, seqNumber); //buferiza o que acabou de enviar
                            fprintf(stderr,"\nPacote %d, conteúdo '%s', enviado e buferizado!\n", seqNumber,buffer);
                            seqNumber = mod((seqNumber + 1),maxSeqNo);
                            byte_count += n;
                        }
                        else {
                            puts("Fim do arquivo.");
                            fclose(arquivo); //fecha o arquivo
                            fim = 1;
                            break;
                        }
                    }
                }
                if (windowFull()&&!fim) puts("Janela cheia. Á espera de acks para enviar mais pacotes...");
                if (!windowEmpty()) { //se a janela já estiver vazia, não há necessidade de se esperarem mais acks
                    ack = 0;
                    timeout = 0;
                    while(!ack&&!timeout) {
                        n = tp_recvfrom(s,received,4+sizeof(int),&cliaddr); //recebe potenciais acks
                        if(n<=0) { //erro
                            puts("erro");
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {//timeout
                                perror("Timeout: ");
                                puts("Reenviando todos os pacotes não confirmados.");
                                timeout = 1;
                                resendAll(&cliaddr); //timeout
                            }
                            else {
                                perror("Erro no recebimento");
                            //caso não tenha dado timeout, continuar iterando
                            }
                        }
                        else {
                            puts("ack?");
                            received[n] = 0;
                            deserializeAck(received,&ackNumber,aux,n);
                            if (!strcmp(aux, "ackn")) { //verifica se é ACK
                                puts("ack!");
                                ack = 1;
                                acknowledge(ackNumber);//acknowledge
                                fprintf(stderr,"\nAck %d recebido!\n",ackNumber);
                                flag = 0;
                                while(removeAckds()){
                                    flag++;
                                }//remove pacotes confirmados da janela
                                fprintf(stderr,"\n%d espaço(s) liberado(s) na janela!\n",flag);
                            }
                        }
                    }
                }
            }
            strcpy(buffer,"fim");//buffer vazio indica o fim do arquivo
            checkResult = checksum(buffer);
            serialize(pkt,seqNumber,checkResult,buffer);
            n = sendto(s, pkt, strlen(pkt),0, (struct sockaddr *)&cliaddr,sizeof(cliaddr)); //envia o pacote
            gettimeofday(&tv1,0);//encera a contagem de tempo
            long total = (tv1.tv_sec - tv0.tv_sec)*1000000 + tv1.tv_usec - tv0.tv_usec; //tempo decorrido, em microssegundos
            printf("\nDesempenho: \nBytes enviados: %d \nTempo decorrido (microssegundos): %ld\nThroughput: %f bytes/segundo\n", byte_count, total, (float)byte_count*1000000/total);
    //    }
    //}

    return 0;
}
