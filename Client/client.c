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

int s, tam_buffer, tam_janela, tam_pkt;
int maxSeqNo, lastRcvd;
struct sockaddr_in6 cliaddr;
int windowIn, windowOut;
struct windowPos *window;

void deserialize (char *pkt, int *seqNumber, unsigned long *hash_no, char *buffer) {//não sei se a palavra "deserializar" existe. procurar.
    int i, a;
    i = 0;
    *seqNumber = 0;
    *hash_no = 0;
    a = sizeof(int);
    while (a) {
        a--;
        *seqNumber += pkt[i]*pow(8,a);
        i++;

    }
    a = sizeof(unsigned long);
    while (a) {
        a--;
        *hash_no += pkt[i]*pow(8,a);
        i++;
    }
    printf("\n%lu",*hash_no);
    a = strlen(pkt) - sizeof(int) - sizeof(unsigned long);
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
    window[windowIn].Rcvd = 1;
    strcpy(window[windowIn].buffer, buffer);
    strcpy(window[windowIn].pkt, pkt);
    windowIn = (windowIn + 1)%tam_janela;
}

int removeRcvds (void) {
    if (window[windowOut].Rcvd) {
        free(window[windowOut].buffer);
        lastRcvd = window[windowOut].seqNumber;
        windowOut = (windowOut + 1)%tam_janela;
        return 1;
    }
    else return 0;
}


int main(int argc, char**argv) {
    int n,ret, byte_count;
    struct addrinfo hints, *res = NULL;
    struct timeval tv0, tv1;
    int seqNumber;
    unsigned long hash_no;
    char *buffer, *pkt, aux[8];
    FILE* arquivo;

    if (argc != 6) {
        printf("Argumentos necessarios: host_servidor, porto_servidor, nome_arquivo, tam_buffer, tam_janela.\n");
        exit(1);
    }

    tam_janela = argv[5];
    tam_buffer = argv[4];
    lastRcvd = -1; //nada foi recebido ainda
    buffer = (char*)malloc(tam_buffer*sizeof(char));
    tam_pkt = tam_buffer + 8;
    pkt = (char*)malloc((tam_pkt)*sizeof(char)); //o pacote terá tamanho = 64 bits (cabeçalho) + tam_buffer

    gettimeofday(&tv0,0); //inicia a contagem de tempo

    bzero(&hints, sizeof(hints)); //limpa a variável


    hints.ai_family = PF_UNSPEC; //tipo do endereço não especificado
    hints.ai_flags = AI_NUMERICHOST;

    ret = getaddrinfo(argv[1], argv[2], &hints, &res);

    if(ret) {
        printf("Endereço inválido.\n");
        exit(1);
    }

    s = socket(res->ai_family,SOCK_DGRAM,0); //cria um socket UDP
    puts("socket criado\n");

    n = sendto(s,argv[3], strlen(argv[3]),0,res->ai_addr,res->ai_addrlen); //envia o nome do arquivo

    while (n==-1) {
        printf("Erro ao enviar mensagem.\n");
        n = sendto(s,argv[3], strlen(argv[3]),0,res->ai_addr,res->ai_addrlen);
    }

    //abre o arquivo que vai ser gravado
    arquivo = fopen(argv[3],"w+");
    if (arquivo == NULL) {
        printf("Erro ao criar o arquivo");
        exit(1);
    }

    //loop recebe o arquivo buffer a buffer até o fim
    byte_count = 0; //início da contagem de bytes recebidos
    while (1) {
        if (!windowFull()) {
            while(n = recvfrom(s,pkt,tam_pkt,0,NULL,NULL)<0){} //recebe mensagem do server + cabeçalho UDP (64 bits = 8 bytes)
            pkt[n] = 0;
            deserialize(pkt, &seqNumber,&hash_no,buffer);
            puts("aqui");
            printf("%d %lu %s", seqNumber, hash_no, buffer);
            //detecta erros
            if(hash_no != hash(buffer)) {
                puts("Erro detectado");
                continue; //se detectou erro, salta para a próxima iteração
            }
            //buferiza
            windowInsert(buffer, pkt, seqNumber);
            removeRcvds();
            fwrite(buffer, 1, strlen(buffer), arquivo); //escreve bytes do buffer no arquivo
            byte_count += n; //atualiza contagem de bytes recebidos
        }
        //envia ack do último recevido em ordem
        if (lastRcvd >= 0) {
            serialize(aux,lastRcvd);
            n = sendto(s,aux,8,0,res->ai_addr,res->ai_addrlen); //envia o nome do arquivo
            while (n==-1) {
                printf("Erro ao enviar ack.\n");
                n = sendto(s,aux,8,0,res->ai_addr,res->ai_addrlen);
            }
        }
    }

    fclose(arquivo);

    close(s); //encerra a conexão

    gettimeofday(&tv1,0); //finaliza a contagem de tempo
    long total = (tv1.tv_sec - tv0.tv_sec)*1000000 + tv1.tv_usec - tv0.tv_usec; //tempo decorrido, em microssegundos
    printf("\nBuffer: \%5lu byte(s) \%10.2f kbps ( \%lu bytes em \%3u.\%06lu s)", (byte_count*1000000)/(1000*total), byte_count, tv1.tv_sec - tv0.tv_sec, tv1.tv_usec - tv0.tv_usec);
    arquivo = fopen("resultados.txt","a"); //abre o arquivo para salvar os dados
    fprintf(arquivo, "\nBuffer: \%5u byte(s) \%10.2f kbps ( \%u bytes em \%3u.\%06u s)", (byte_count*1000000)/(1000*total), byte_count, tv1.tv_sec - tv0.tv_sec, tv1.tv_usec - tv0.tv_usec);
    fclose(arquivo);
    return 0;
}
