#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>

void mysettimer(void) {
    struct itimerval newvalue, oldvalue;
    newvalue.it_value.tv_sec  = 1; //1 segundo de temporização
    newvalue.it_value.tv_usec = 0;
    newvalue.it_interval.tv_sec  = 0;
    newvalue.it_interval.tv_usec = 0;
    setitimer(ITIMER_REAL, &newvalue, &oldvalue);
}

void timer_handler(struct windowPos *window, int seqNumber, int windowOut, struct sockaddr_in6 *cliaddr) {
    resend(window, seqNumber, windowOut, cliaddr); //reenvia o pacote não confirmado
    mysettimer(espera);  //reinicia o timer
}

void mysethandler(struct windowPos *window, int seqNumber, int windowOut, struct sockaddr_in6 *cliaddr) {
    signal(SIGALRM,timer_handler(seqNumber));
}

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

void windowInsert (struct windowPos* window, int tam_buffer, char *buffer, int seqNumber, int tam_janela, int *windowIn) {
    window[*windowIn].buffer = (char*)malloc(tam_buffer);
    window[*windowIn].seqNumber = seqNumber;
    window[*windowIn].AckRcvd = 0;
    strcpy(window[*windowIn].buffer, buffer);
    *windowIn = (*windowIn + 1)%tam_janela;
}

int removeAckds (struct windowPos *window, int *windowOut, int tam_janela) {
    if (window[*windowOut].AckRcvd) {
        free(window[*windowOut].buffer);
        *windowOut = (*windowOut + 1)%tam_janela;
        return 1;
    }
    else return 0;
}

void acknowledge (struct windowPos *window, int windowOut, int seqNumber, int maxSeqNo) {
    int num;
    if (seqNumber < window[windowOut].seqNumber) seqNumber+=maxSeqNo;
    num = seqNumber - window[windowOut].seqNumber;
    window[windowOut + num].AckRcvd = 1;
}

void resend (struct windowPos *window, int seqNumber, int windowOut, struct sockaddr_in6 *cliaddr) {
        int num;
        if (seqNumber < window[windowOut].seqNumber) seqNumber+=2*maxSeqNo;
        num = seqNumber - window[windowOut].seqNumber;
        sendto(s, window[windowOut + num].buffer, strlen(window[windowOut + num].buffer),0, (struct sockaddr *)cliaddr,sizeof(cliaddr));

}


int main(int argc, char**argv) {
    int s, ret, len, n, tam_buffer, byte_count, tam_janela, maxSeqNo, seqNumber;
    unsigned int ackNumber;
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
    windowInit(&windowIn, &windowOut); //inicializa a janela

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
                if(!windowFull(tam_janela, windowIn, windowOut)) {
                    if(fread(buffer, 1, tam_buffer, arquivo)) {
                        n = sendto(s, buffer, strlen(buffer),0, (struct sockaddr *)&cliaddr,sizeof(cliaddr));
        -               puts(buffer);
                        windowInsert(window, tam_buffer, buffer, seqNumber, tam_janela, &windowIn);
                        mysethandler(window, seqNumber, windowOut, &cliaddr); //seta o handler de temporização para esse pacote
                        mysettimer();
                        seqNumber = (seqNumber + 1)%maxSeqNo;
                        byte_count += n;
                    }
                    else break; //fim do arquivo
                }
                n = recvfrom(s,received,2,0,(struct sockaddr *)&cliaddr, &len);
                received[n] = 0;
                if (received[0]=='A') {
                    ackNumber = unsigned int(received[1]); //converte o número de sequência enviado no ack para inteiro
                    acknowledge(window, windowOut, ackNumber, maxSeqNo);//acknowledge
                    removeAckds(window, &windowOut, tam_janela);//remove pacotes confirmados da janela
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
