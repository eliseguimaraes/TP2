#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>

struct windowPos {
    long int seqNumber;
    int Rcvd;
    char *buffer;
    char *pkg;
};

int s, tam_buffer, tam_janela, tam_pkg;
long int maxSeqNo, lastRcvd;
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

void mysethandler(void) {
    signal(SIGALRM,timer_handler);
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
    window[windowIn].Rcvd = 1;
    strcpy(window[windowIn].buffer, buffer);
    strcpy(window[windowIn].pkg, pkg);
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

void acknowledge () { //confirma o último pacote recebido em ordem
        char string[8];
        sendto(s, window[windowOut + num].pkg, strlen(window[windowOut + num].pkg),0, (struct sockaddr *)cliaddr,sizeof(cliaddr));
}


int main(int argc, char**argv) {
    int n,ret, byte_count;
    struct addrinfo hints, *res = NULL;
    struct timeval tv0, tv1;
    long int seqNumber, ackNumber;
    unsigned long hash_no;
    char *buffer, *pkg, aux[8];
    FILE* arquivo;

    if (argc != 6) {
        printf("Argumentos necessarios: host_servidor, porto_servidor, nome_arquivo, tam_buffer, tam_janela.\n");
        exit(1);
    }

    tam_janela = argv[5];
    tam_buffer = argv[4];
    lastRcvd = -1; //nada foi recebido ainda
    buffer = (char*)malloc(tam_buffer*sizeof(char));
    tam_pkg = tam_buffer + 8;
    pkg = (char*)malloc((tam_pkg)*sizeof(char)); //o pacote terá tamanho = 64 bits (cabeçalho) + tam_buffer

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

    puts(argv[3]);

    //abre o arquivo que vai ser gravado
    arquivo = fopen(argv[3],"w+");
    if (arquivo == NULL) {
        printf("Erro ao criar o arquivo");
        exit(1);
    }

    //loop recebe o arquivo buffer a buffer até o fim
    byte_count = 0; //início da contagem de bytes recebidos
    while (1) {
        while(n = recvfrom(s,pkg,tam_pkg,0,NULL,NULL)<0){} //recebe mensagem do server + cabeçalho UDP (64 bits = 8 bytes)
        pkg[n] = 0;
        strncpy(aux,pkg,8);//copia a parte correspondente à correção
        hash_no = ntohl(aux);
        strncpy(aux, pkg+8,8);//copia a parte correspondente ao número de sequência
        seqNumber = ntohl(aux);
        strcpy(buffer, pkg+16); //copia o restante para buferizar
        //detecta erros
        if(hash_no != hash(buffer)) {
            puts("Erro detectado");
            continue; //se detectou erro, salta para a próxima iteração
        }
        //buferiza
        windowInsert(buffer, pkg, seqNumber);

        fwrite(buffer, 1, strlen(buffer), arquivo); //escreve bytes do buffer no arquivo
        byte_count += n; //atualiza contagem de bytes recebidos
    }

    fclose(arquivo);

    close(s); //encerra a conexão

    gettimeofday(&tv1,0); //finaliza a contagem de tempo
    long total = (tv1.tv_sec - tv0.tv_sec)*1000000 + tv1.tv_usec - tv0.tv_usec; //tempo decorrido, em microssegundos
    printf("\nBuffer: \%5u byte(s) \%10.2f kbps ( \%u bytes em \%3u.\%06u s)", (byte_count*1000000)/(1000*total), byte_count, tv1.tv_sec - tv0.tv_sec, tv1.tv_usec - tv0.tv_usec);
    arquivo = fopen("resultados.txt","a"); //abre o arquivo para salvar os dados
    fprintf(arquivo, "\nBuffer: \%5u byte(s) \%10.2f kbps ( \%u bytes em \%3u.\%06u s)", (byte_count*1000000)/(1000*total), byte_count, tv1.tv_sec - tv0.tv_sec, tv1.tv_usec - tv0.tv_usec);
    fclose(arquivo);
    return 0;
}
