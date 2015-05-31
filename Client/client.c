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
    char *packet;
};

int s, tam_buffer, tam_janela;
long int seqNumber, ackNumber, maxSeqNo
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

void mysettimer(void) {
    struct itimerval newvalue, oldvalue;
    newvalue.it_value.tv_sec  = 1; //1 segundo de temporização
    newvalue.it_value.tv_usec = 0;
    newvalue.it_interval.tv_sec  = 0;
    newvalue.it_interval.tv_usec = 0;
    setitimer(ITIMER_REAL, &newvalue, &oldvalue);
}

void timer_handler(int signum) {
    int num;
    resend(seqNumber); //reenvia o pacote não confirmado
    mysettimer(espera);  //reinicia o timer caso o ack ainda não tenha sido recebido
}

void mysethandler(void) {
    signal(SIGALRM,timer_handler);
}

void windowInit(void) {
    int i;
    windowIn = windowOut = 0;
    while(!windowFull()) {

    }
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

void windowInsert (char *buffer, char* packet, long int seqNumber) {
    window[windowIn].buffer = (char*)malloc(tam_buffer);
    window[windowIn].seqNumber = seqNumber;
    window[windowIn].AckRcvd = 0;
    strcpy(window[windowIn].buffer, buffer);
    strcpy(window[windowIn].packet, packet);
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

void acknowledge (long int seqNumber) {
    int num;
    if (seqNumber < window[windowOut].seqNumber) seqNumber+=maxSeqNo;
    num = seqNumber - window[windowOut].seqNumber;
    window[windowOut + num].AckRcvd = 1;
}

void resend (long int seqNumber) {
        int num;
        if (seqNumber < window[windowOut].seqNumber) seqNumber+=2*maxSeqNo;
        num = seqNumber - window[windowOut].seqNumber;
        sendto(s, window[windowOut + num].packet, strlen(window[windowOut + num].packet),0, (struct sockaddr *)cliaddr,sizeof(cliaddr));
}


int main(int argc, char**argv) {
    int n,ret, byte_count;
    struct addrinfo hints, *res = NULL;
    struct timeval tv0, tv1;
    unsigned long hash_no;
    char *buffer, *pkg;
    FILE* arquivo;

    flag = 0;

    if (argc != 6) {
        printf("Argumentos necessarios: host_servidor, porto_servidor, nome_arquivo, tam_buffer, tam_janela.\n");
        exit(1);
    }

    tam_janela = argv[6];
    tam_buffer = argv[5];
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
        n = recvfrom(s,pkg,tam_pkg,0,NULL,NULL); //recebe mensagem do server + cabeçalho UDP (64 bits = 8 bytes)
        pkg[n] = 0;
        //detecta erros
        //lê número de sequência
        //buferiza
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
