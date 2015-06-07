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

struct windowPos {
    unsigned int seqNumber;
    int Rcvd;
    char *buffer;
    char *pkt;
};

int s;
unsigned int tam_buffer, tam_janela, tam_pkt, windowRoom;
unsigned int maxSeqNo, lastRcvd;
unsigned int windowIn, windowOut;
struct windowPos *window;

int mod(int a, int b) { //aritmética modular
    int r = a % b;
    return r<0 ? (r+b):r;
}

void deserialize (unsigned char *pkt, unsigned int *seqNumber, unsigned int *checkResult, char *buffer, unsigned int n) {//não sei se a palavra "deserializar" existe. procurar.
    unsigned int i, a;
    i = 0;
    *seqNumber = 0;
    *checkResult = 0;
    a = sizeof(unsigned int);
    for (i=0; i<a; i++) {
        *seqNumber += (pkt[i]-1)*pow(256,i);
        *checkResult += (pkt[i+a]-1)*pow(256,i);
    }
    i = 2*sizeof(unsigned int);
    a = n - 2*sizeof(unsigned int);
    buffer[a] = 0;
    while(a) {
        a--;
        buffer[i-8] = pkt[i];
        i++;
    }
}

void serializeAck (unsigned char *ack, unsigned int lastRcvd) { //serialização dos acks
    unsigned int i, a;
    unsigned char string[4];
    strcpy(string, "ackn");
    a = sizeof(int);
    for (i=0; i<a; i++)  {//serializa o inteiro
        ack[i] = (lastRcvd) >> 8*i;
        ack[i]+=1;
    }
    a = strlen(string);
    while (a) { //serializa a string
        a--;
        ack[i] = string[i-4];
        i++;
    }
    ack[i] = 0;
}

unsigned int checksum (char *str) {
    unsigned int i;
    unsigned int checkResult = 0;
    for (i=0; str[i]!='\0'; i++)
        checkResult += (unsigned int)str[i];
    return checkResult;
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

void windowReserve (unsigned int seqNumber) { //cria espaços na janela para buferizar novos elementos
        window[windowIn].seqNumber = seqNumber;
        window[windowIn].Rcvd = 0;
        windowIn = mod((windowIn + 1),tam_janela);
        windowRoom++;
}


void windowInit ()  { //inicializa a janela
    windowIn = 0;
    windowOut = 0;
    unsigned int i;
    for (i=0; i<tam_janela; i++) {
        windowReserve(i);
        window[i].buffer = (char*)malloc(tam_buffer);
        window[i].pkt = (char*)malloc(tam_pkt);
    }
    windowRoom = tam_janela;
}

void windowEnd () {
    unsigned int i;
    for (i=0; i<tam_janela; i++) {
        free(window[i].buffer);
        free(window[i].pkt);
    }
}

void windowStore (char *buffer, char *pkt, unsigned int seqNumber) {
        int num;
        num = mod((seqNumber - window[windowOut].seqNumber),maxSeqNo);
        if (num < tam_janela) {
            strcpy(window[windowOut + num].buffer,buffer);
            strcpy(window[windowOut + num].pkt,pkt);
            window[windowOut + num].Rcvd = 1;
            windowRoom--;
        }
}
int removeRcvds (void) {
    if (window[windowOut].Rcvd) {
        lastRcvd = window[windowOut].seqNumber;
        windowOut = mod((windowOut + 1),tam_janela);
        return 1;
    }
    else return 0;
}

int main(int argc, char**argv) {
    int n,ret,i;
    unsigned int nextSeq, checkResult, seqNumber;
    unsigned long buffer_total,byte_count;
    so_addr servaddr;
    struct timeval tv0, tv1;
    char *buffer, *pkt, ack[8],*fileName;
    FILE* arquivo;

    if (tp_init())//erro
        exit(1);

    if (argc != 6) {
        fprintf(stderr,"Argumentos necessarios: host_servidor, porto_servidor, nome_arquivo, tam_buffer, tam_janela.\n");
        exit(1);
    }

    tam_janela = atoi(argv[5]);
    tam_buffer = atoi(argv[4]);
    lastRcvd = -1; //nada foi recebido ainda
    maxSeqNo = 2*tam_janela; //há o dobro de números de sequência que posições na janela
    window = (struct windowPos*)malloc(tam_janela*sizeof(struct windowPos));
    buffer = (char*)malloc(tam_buffer*sizeof(char));
    tam_pkt = tam_buffer + 2*sizeof(int);
    pkt = (char*)malloc(tam_pkt); //o pacote terá tamanho = 8 bytes (cabeçalho) + tam_buffer

    gettimeofday(&tv0,0); //inicia a contagem de tempo

    ret = tp_build_addr(&servaddr,argv[1],atoi(argv[2]));

    if(ret) {
        fprintf(stderr,"Endereço inválido.\n");
        exit(1);
    }

    s = socket(PF_INET,SOCK_DGRAM,0); //cria um socket UDP
    puts("socket criado\n");
    fileName = (char*)malloc(strlen(argv[3]+1));
    //detecção de erro (rudimentar) para nome do arquivo
    fileName[0] = 0;
    for (i=0; argv[3][i]!='\0'; i++) fileName[0] = (fileName[0]+argv[3][i])%255;
    fileName[1] = '\0';
    strcat(fileName,argv[3]);
    n = tp_sendto(s,fileName, strlen(fileName),&servaddr); //envia o nome do arquivo
    while (n==-1) {
        fprintf(stderr,"Erro ao enviar mensagem.\n");
        n = tp_sendto(s,fileName, strlen(fileName),&servaddr);
    }
    free(fileName);
    fprintf(stderr,"\nArquivo solicitado: %s\n",argv[3]);

    //abre o arquivo que vai ser gravado
    arquivo = fopen(argv[3],"w+");
    if (arquivo == NULL) {
        fprintf(stderr,"Erro ao criar o arquivo");
        exit(1);
    }
    //loop recebe o arquivo buffer a buffer até o fim
    byte_count = 0; //início da contagem de bytes recebidos
    buffer_total = 0;
    windowInit();
    while (1) {
        if (windowRoom) {
            n = tp_recvfrom(s,pkt,tam_pkt,NULL);
            while (n<8) {
                n = tp_recvfrom(s,pkt,tam_pkt,NULL);
            }
            pkt[n] = 0;
            deserialize(pkt, &seqNumber,&checkResult,buffer, n);
	        buffer_total += strlen(buffer);
	        printf("\nBytes recebidos até o momento: %lu.\n",buffer_total);
            if(checkResult != checksum(buffer)) {//detecta erros
                puts("Erro detectado");
                continue; //se detectou erro, salta para a próxima iteração
            }
            if (!strcmp(buffer, "fim")) {
                puts("Fim do arquivo");
                    fclose(arquivo);
                    windowEnd();
                    free(buffer);
                    free(pkt);
                    free(window);
                    close(s); //encerra a conexão
                    gettimeofday(&tv1,0); //finaliza a contagem de tempo
                    long total = (tv1.tv_sec - tv0.tv_sec)*1000000 + tv1.tv_usec - tv0.tv_usec; //tempo decorrido, em microssegundos
                    fprintf(stderr,"\nTamanho do arquivo: \%5lu bytes Tamanho do buffer: \%5lu byte(s) Tamanho da janela: \%d \%10.2f kbps ( \%lu bytes em \%3u.\%06lu s)\n", buffer_total,tam_buffer,tam_janela,(double)(byte_count*1000000)/(double)(1000*total), byte_count, (unsigned int)(tv1.tv_sec - tv0.tv_sec), tv1.tv_usec - tv0.tv_usec);
                    arquivo = fopen("resultados.txt","a"); //abre o arquivo para salvar os dados
                    fprintf(arquivo,"\nTamanho do arquivo: \%5lu bytes Tamanho do buffer: \%5lu byte(s) Tamanho da janela: \%d \%10.2f kbps ( \%lu bytes em \%3u.\%06lu s)", buffer_total,tam_buffer,tam_janela,(double)(byte_count*1000000)/(double)(1000*total), byte_count, (unsigned int)(tv1.tv_sec - tv0.tv_sec), tv1.tv_usec - tv0.tv_usec);
                    fclose(arquivo);
                    return 0;
            }
            //buferiza
            windowStore(buffer, pkt, seqNumber);
            while(removeRcvds()){}
            nextSeq = mod((window[mod((windowIn - 1),tam_janela)].seqNumber + 1),maxSeqNo);
            while (!windowFull()) {
                windowReserve(nextSeq);
                nextSeq++;
            }
            fprintf(stderr,"\nPacote %d recebido!",seqNumber);
            fwrite(buffer, 1, strlen(buffer), arquivo); //escreve bytes do buffer no arquivo
            fprintf(stderr,"\n'%s' escrito no arquivo\n",buffer);
            byte_count += n; //atualiza contagem de bytes recebidos
        }
        else puts("Janela cheia. Aguardando pacotes em ordem...");
        //envia ack do último recebido em ordem
        if (lastRcvd >= 0) {
            serializeAck(ack,lastRcvd); //cria o pacote do ack
            n = tp_sendto(s,ack,8,&servaddr); //envia o nome do arquivo
            while (n==-1) {
                fprintf(stderr,"Erro ao enviar ack.\n");
                n = tp_sendto(s,ack,8,&servaddr);
            }
            fprintf(stderr,"Ack %d enviado!",lastRcvd,n);
        }
    }
    fclose(arquivo);
    windowEnd();
    free(buffer);
    free(pkt);
    free(window);
    close(s); //encerra a conexão
    return 0;
}
