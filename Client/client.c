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

struct windowPos {
    int seqNumber;
    int Rcvd;
    char *buffer;
    char *pkt;
};

int s, tam_buffer, tam_janela, tam_pkt, windowRoom;
int maxSeqNo, lastRcvd;
struct sockaddr_in6 cliaddr;
int windowIn, windowOut;
struct windowPos *window;

void deserialize (char *pkt, int *seqNumber, int *checkResult, char *buffer, int n) {//não sei se a palavra "deserializar" existe. procurar.
    int i, a;
    i = 0;
    *seqNumber = 0;
    *checkResult = 0;
    a = sizeof(int);
    while (a) {
        a--;
        *seqNumber += pkt[i]*pow(8,a);
        i++;

    }
    a = sizeof(int);
    while (a) {
        a--;
        *checkResult += pkt[i]*pow(8,a);
        i++;
    }
    a = n - 2*sizeof(int);
    fprintf(stderr,"%d",a);
    buffer[a+1] = 0;
    while(a) {
        a--;
        buffer[a] = pkt[i];
        i++;
    }
}

void serialize (char *aux, int lastRcvd) { //serialização dos acks
    int i, a;
    char string[4];
    strcpy(string, "ackn");
    a = sizeof(int);
    i = 0;
    while (a) { //serializa o inteiro
        a--;
        aux[i] = lastRcvd >> a*8;
        i++;
    }
    a = strlen(string);
    while (a) { //serializa a string
        a--;
        aux[i] = string[a];
        i++;
    }
    aux[i] = 0;
}

int checksum (char *str) {
    int i, checkResult = 0;
    for (i=0; str[i]!='\0'; i++)
        checkResult += (int)str[i];
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

void windowReserve (int seqNumber) { //cria espaços na janela para buferizar novos elementos
        window[windowIn].buffer = (char*)malloc(tam_buffer);
        window[windowIn].pkt = (char*)malloc(tam_pkt);
        window[windowIn].seqNumber = seqNumber;
        window[windowIn].Rcvd = 0;
        windowIn = (windowIn + 1)%tam_janela;
        windowRoom++;
}


void windowInit ()  { //inicializa a janela
    windowIn = 0;
    windowOut = 0;
    int i;
    for (i=0; i<tam_janela; i++)
        windowReserve(i);

    windowRoom = tam_janela;
}

void windowStore (char *buffer, char *pkt, int seqNumber) {
        int num;
        num = (seqNumber - window[windowOut].seqNumber)%maxSeqNo;
        if (num < tam_janela) {
            strcpy(window[windowOut + num].buffer,buffer);
            strcpy(window[windowOut + num].pkt,pkt);
            window[windowOut + num].Rcvd = 1;
            windowRoom--;
        }
}
int removeRcvds (void) {
    if (window[windowOut].Rcvd) {
        free(window[windowOut].buffer);
        free(window[windowOut].pkt);
        lastRcvd = window[windowOut].seqNumber;
        windowOut = (windowOut + 1)%tam_janela;
        return 1;
    }
    else return 0;
}


int main(int argc, char**argv) {
    int n,ret, byte_count, nextSeq, checkResult;
    struct addrinfo hints, *res = NULL;
    struct timeval tv0, tv1;
    int seqNumber;
    unsigned long hash_no;
    char *buffer, *pkt, aux[8];
    FILE* arquivo;

    if (argc != 6) {
        fprintf(stderr,"Argumentos necessarios: host_servidor, porto_servidor, nome_arquivo, tam_buffer, tam_janela.\n");
        exit(1);
    }

    tam_janela = atoi(argv[5]);
    tam_buffer = atoi(argv[4]);
    lastRcvd = -1; //nada foi recebido ainda
    window = (struct windowPos*)malloc(tam_janela*sizeof(struct windowPos));
    buffer = (char*)malloc(tam_buffer*sizeof(char));
    tam_pkt = tam_buffer + 2*sizeof(int);
    pkt = (char*)malloc(tam_pkt); //o pacote terá tamanho = 8 bytes (cabeçalho) + tam_buffer

    gettimeofday(&tv0,0); //inicia a contagem de tempo

    bzero(&hints, sizeof(hints)); //limpa a variável


    hints.ai_family = PF_UNSPEC; //tipo do endereço não especificado
    hints.ai_flags = AI_NUMERICHOST;

    ret = getaddrinfo(argv[1], argv[2], &hints, &res);

    if(ret) {
        fprintf(stderr,"Endereço inválido.\n");
        exit(1);
    }

    s = socket(res->ai_family,SOCK_DGRAM,0); //cria um socket UDP
    puts("socket criado\n");

    n = sendto(s,argv[3], strlen(argv[3]),0,res->ai_addr,res->ai_addrlen); //envia o nome do arquivo

    while (n==-1) {
        fprintf(stderr,"Erro ao enviar mensagem.\n");
        n = sendto(s,argv[3], strlen(argv[3]),0,res->ai_addr,res->ai_addrlen);
    }

    //abre o arquivo que vai ser gravado
    arquivo = fopen(argv[3],"w+");
    if (arquivo == NULL) {
        fprintf(stderr,"Erro ao criar o arquivo");
        exit(1);
    }
    //loop recebe o arquivo buffer a buffer até o fim
    byte_count = 0; //início da contagem de bytes recebidos
    windowInit();
    while (1) {
        if (windowRoom) {
            fprintf(stderr,"%d", windowRoom);
            while(n = recvfrom(s,pkt,tam_pkt,0,NULL,NULL)<=0) {}//recebe mensagem do server + cabeçalho UDP
            pkt[n] = 0;
            puts("aqui");
            //fprintf(stderr,"    %d    ", n);
            deserialize(pkt, &seqNumber,&checkResult,buffer, n);
            //detecta erros
            if(checkResult != checksum(buffer)) {
                puts("Erro detectado");
                continue; //se detectou erro, salta para a próxima iteração
            }
            //buferiza
            windowStore(buffer, pkt, seqNumber);
            while(removeRcvds());
            nextSeq = (window[windowIn - 1].seqNumber + 1)%maxSeqNo;
            while (!windowFull) {
                windowReserve(nextSeq);
                nextSeq++;
            }
            fwrite(buffer, 1, strlen(buffer), arquivo); //escreve bytes do buffer no arquivo
            byte_count += n; //atualiza contagem de bytes recebidos
        }
        //envia ack do último recevido em ordem
        if (lastRcvd >= 0) {
            serialize(aux,lastRcvd);
            n = sendto(s,aux,8,0,res->ai_addr,res->ai_addrlen); //envia o nome do arquivo
            while (n==-1) {
                fprintf(stderr,"Erro ao enviar ack.\n");
                n = sendto(s,aux,8,0,res->ai_addr,res->ai_addrlen);
            }
        }
    }

    fclose(arquivo);
    free(buffer);
    free(pkt);

    close(s); //encerra a conexão

    gettimeofday(&tv1,0); //finaliza a contagem de tempo
    long total = (tv1.tv_sec - tv0.tv_sec)*1000000 + tv1.tv_usec - tv0.tv_usec; //tempo decorrido, em microssegundos
    fprintf(stderr,"\nBuffer: \%5lu byte(s) \%10.2f kbps ( \%lu bytes em \%3u.\%06lu s)", (byte_count*1000000)/(1000*total), byte_count, tv1.tv_sec - tv0.tv_sec, tv1.tv_usec - tv0.tv_usec);
    arquivo = fopen("resultados.txt","a"); //abre o arquivo para salvar os dados
    ffprintf(stderr,arquivo, "\nBuffer: \%5u byte(s) \%10.2f kbps ( \%u bytes em \%3u.\%06u s)", (byte_count*1000000)/(1000*total), byte_count, tv1.tv_sec - tv0.tv_sec, tv1.tv_usec - tv0.tv_usec);
    fclose(arquivo);
    return 0;
}
