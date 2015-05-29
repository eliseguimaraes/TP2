#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>

int main(int argc, char**argv) {
    int s,n,ret, byte_count, tam_buffer;
    struct addrinfo hints, *res = NULL;
    struct timeval tv0, tv1;
    char buffer[256];
    FILE* arquivo;

    if (argc != 6) {
        printf("Argumentos necessarios: host_servidor, porto_servidor, nome_arquivo, tam_buffer, tam_janela.\n");
        exit(1);
    }

    gettimeofday(&tv0,0); //inicia a contagem de tempo

    //conexão com o servidor

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
    tam_buffer = argv[5];
    byte_count = 0; //início da contagem de bytes recebidos
    while (1) {
        n = recvfrom(s,buffer,tam_buffer,0,NULL,NULL); //recebe mensagem do server
        buffer[n] = 0;
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
