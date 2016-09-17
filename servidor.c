#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include "parser.tab.c"
#include "lex.yy.c"

int escutasocket(unsigned short port);
void signal_handler();
void *servico(void *msgsock);
char* concat(char *s1, char *s2);
char *extensao(char *filename);
void tempo();
char * semnewline(char *s);
char *respostahttp(struct comando *c);
char *webspace;
FILE *registro;
int threads, MAXTHREADS;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


main(int argc, char *argv[]){

	char *saida;
	int pid, sock, msgsock, n=0;
	unsigned short port;
	MAXTHREADS = 50;
	threads=0;
	pthread_t thread;
	pthread_attr_t attr;
	
	webspace = argv[2];
	
	port = atoi(argv[1]);

	if ((registro = fopen(argv[3], "ab")) == -1){
		printf("Erro na abertura do arquivo registro\n");
		exit(-1);
	}

	sock = escutasocket(port);
	
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);


	while (1){
		if((msgsock = accept(sock,(struct sockaddr *) 0,(int *) 0)) != -1){
			if(threads >= MAXTHREADS){ 
				saida = respostahttp("unavailable");
				printf("Unavailable (Número de threads: %d, MAX %d):\n%s\n", threads, MAXTHREADS, saida);
				write(msgsock, saida, strlen(saida));
				close(msgsock);
				continue;
			} else {
				pthread_mutex_lock(&mutex);
				threads++;
				pthread_mutex_unlock(&mutex);
				pthread_create(&thread, &attr, servico, (void *)(intptr_t)msgsock);
			}

		} else {
			if(errno == EINTR) continue;
			perror("accept");
		}
	}
}

void *servico(void *sock_e){

    struct timeval timeout;
    long int tolerancia=2;
    int fd = 0, n, pid;
    char entrada[1024];
    char *saida;
    fd_set fds;
    int deletar=0;
    int bytes, sock = (intptr_t)sock_e;
    struct comando *c;

    timeout.tv_sec = tolerancia;
    timeout.tv_usec = 0;


    while(1){

	    FD_ZERO(&fds);  
 	    FD_SET(sock, &fds); 

	    n = select(sock+1, &fds, (fd_set *)0, (fd_set *)0, &timeout);
	    if(n == -1){
	    	perror("Erro em select():");
	    	close(sock);
		pthread_mutex_lock(&mutex);
	    	threads--;
		pthread_mutex_unlock(&mutex);
	    	pthread_exit(NULL);
	    } else {
	    	if(n == 0){
			pthread_mutex_lock(&mutex);
	    		threads--;
			pthread_mutex_unlock(&mutex);
	    		close(sock);
	    		pthread_exit(NULL);
	    	} else {
			memset(entrada, 0, sizeof entrada);
			do{
			  if ((bytes = read(sock, entrada, sizeof(entrada))) == -1) perror("Reading stream message");
			} while ( bytes > 1024);
	  		c=NULL;
	  		//if(strlen(entrada)<2) continue;
	  		if(strlen(entrada)<2) pthread_exit(NULL);
	  		yy_scan_string(entrada);
			c = yyparser();
			imprimir(c, registro);
  			saida = respostahttp(c);
  			printf("%s\n\n", saida);
  			fsync(1);
  			//if(strcmp(conexao, "keep-alive")) tolerancia = 0;
  			//else timeout.tv_sec = tolerancia;
  			timeout.tv_sec = tolerancia;
  			write(sock, saida, strlen(saida));
	    	}
	    }
    }
	
}

int escutasocket(unsigned short port){

    int sock, length;
    struct sockaddr_in server;
    char buf[1024];
    int msgsock;

    /* Create socket. */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("opening stream socket");
        exit(1);
    }

    /* Bind socket using wildcards.*/
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);
    if (bind(sock, (struct sockaddr *) &server, sizeof server)
        == -1) {
        perror("binding stream socket");
        exit(1);
    }
    /* Find out assigned port number and print it out. */
    length = sizeof server;
    if (getsockname(sock,(struct sockaddr *) &server,&length)
          == -1) {
        perror("getting socket name");
        exit(1);
    }
    /* Start accepting connections. */
    listen(sock, 5);

    return sock;

}

char *respostahttp(struct comando *c){
	char *requisicao = c->nomecomando;
	char *conexao = "Close";
	time_t tempo;
	tempo=time(NULL);
	int status=0;
	char *recurso, *reci, *recw, *ext=NULL;
	char *dir;
	char buf;
	struct stat statbuf;
	FILE *fd;
	char aux[100];
	char *saida = (char *)malloc(sizeof(char)*100000);

	memset(saida, 0, sizeof saida);

	//Requisição: TRACE
	if(!strcmp(requisicao, "TRACE")){
		struct comando *cc=c;
		fprintf(registro, "%s ", cc->nomecomando);
		strcat(saida, cc->nomecomando);
		strcat(saida, " ");
	    while (1){ 
	    	struct parametro *aux = cc->nextp;
	    	while(aux!=NULL){
		    	if(aux->next != NULL) {
		    		fprintf(registro, "%s ", aux->nomeparametro);
		    		strcat(saida, aux->nomeparametro);
		    		strcat(saida, " ");
		    	} else {
		    		fprintf(registro, "%s \r\n", aux->nomeparametro);
		    		strcat(saida, aux->nomeparametro);
		    		strcat(saida, " \r\n");
		    	}
		    	aux=aux->next;
		    }
			cc=cc->next;
			if(cc==NULL) break;
		    fprintf(registro, "%s: ", cc->nomecomando);
		    strcat(saida, cc->nomecomando);
		    strcat(saida, ": ");
	    }
	    fprintf(registro, "\r\n");
	    strcat(saida, "\r\n");
	    return saida;
    }


    //Extraindo o tipo de conexão
    struct comando *cc=c;
    while(cc!=NULL){
    	if(!strcmp("Connection", cc->nomecomando)){
    		if (cc->nextp->nomeparametro != NULL) conexao = cc->nextp->nomeparametro;
    		break;
    	}
    	cc=cc->next;
    }

    //Obtendo o recurso requisitado
    if(c!= NULL && strcmp(c->nomecomando, "erro")) dir = strtok(c->nextp->nomeparametro, " ");

    //Requisição: OPTIONS
    if(!strcmp(requisicao, "OPTIONS")){
		recurso = concat(webspace, dir);
		sprintf(saida, "HTTP/1.1 200 OK\r\nDate: %s BRT\r\nServer: Servidor HTTP do Farzin\r\nConnection: %s\r\nAllow: GET, HEAD, TRACE, OPTIONS\r\n\r\n", semnewline(asctime(localtime(&tempo))),conexao);
		fprintf(registro, "HTTP/1.1 200 OK\r\nDate: %s BRT\r\nServer: Servidor HTTP do Farzin\r\nConnection: %s\r\nAllow: GET, HEAD, TRACE, OPTIONS\r\n\r\n", semnewline(asctime(localtime(&tempo))),conexao);
		return saida;
	}


	if(strcmp(requisicao,"GET") && strcmp(requisicao,"HEAD")){//Requisição diferente de get ou head?
		if (!strcmp(requisicao,"erro")) status = 400; //Sim, requisição igual a "erro"? Se sim, Status 400
		else if (!strcmp(requisicao,"unavailable")) status = 503; //Service Unavailable
		else status = 501; //Se não, status 501
	} else { //Requisição get ou head. Determina as permissões de execução e leitura
		recurso = concat(webspace, dir);
		if(stat(recurso, &statbuf) != -1){ //ARQUIVO EXISTE?
		      if ((statbuf.st_mode & S_IFMT) == S_IFDIR){ //DIRETÓRIO
				if(statbuf.st_mode & S_IXUSR){ //DIRETÓRIO EXECUTÁVEL
				    reci = concat(recurso, "/index.html");
				    if(access(reci, F_OK) != -1){
					  if(!access(reci, R_OK)) {
						  status = 200;
						  stat(reci, &statbuf);
						  recurso = reci;
					  } else {
						  status = 403;
					  }
				    } else {
					  status = 404; //INDEX NAO EXISTE
				    }
				    if (status == 403 || status == 404){
					  recw = concat(recurso, "/welcome.html");
					  if(access(recw, F_OK) != -1){
					      if(!access(recw, R_OK)) {
						      status = 200;
						      stat(recw, &statbuf);
						      recurso = recw;
					      } else {
						      status = 403;
					      }
					  } else {
					    status = 404; //WELCOME NAO EXISTE
					  }
				    }
				} else { //DIRETÓRIO NÃO EXECUTÁVEL
				    status = 403;
				}
		      } else { //ARQUIVO
			    if(!access(recurso, R_OK)) {
				    status = 200;
			    } else {
				    status = 403;
			    }
		      }
		} else { //ARQUIVO NAO EXISTE
			status = 404; 
		}
	}

	//Switch associando o status com o que deve ser impresso. Escreve-se no port de volta ao requisitante e em ambos os arquivos providos
	switch (status){
		case 200:
			fd = fopen(recurso, "r");
			ext = extensao(recurso);
			printf("EXT É %s\n", ext);
			if(!strcmp(ext, "html")) ext = concat("text/", "html");
			else if(!strcmp(ext, "pdf")) ext = concat("application/", "pdf");
			else if(!strcmp(ext, "png")) ext = concat("image/", "png");
			else if(!strcmp(ext, "jpg")) ext = concat("image/", "jpg");
			else if(!strcmp(ext, "gif")) ext = concat("image/", "gif");
			else if(!strcmp(ext, "txt")) ext = concat("text/", "html");
			sprintf(saida, "HTTP/1.1 200 OK\r\nDate: %s BRT\r\nServer: Servidor HTTP do Farzin\r\nConnection: %s \r\nLast-Modified: %s BRT\r\nContect-Length: %ld \r\nContent-Type: %s \r\n\r\n", semnewline(asctime(localtime(&tempo))), conexao, semnewline(asctime(localtime(&statbuf.st_mtime))),(long)statbuf.st_size, ext);
			fprintf(registro, "HTTP/1.1 200 OK\r\nDate: %s BRT\r\nServer: Servidor HTTP do Farzin\r\nConnection: %s \r\nLast-Modified: %s BRT\r\nContect-Length: %ld \r\nContent-Type: %s \r\n\r\n", semnewline(asctime(localtime(&tempo))), conexao, semnewline(asctime(localtime(&statbuf.st_mtime))),(long)statbuf.st_size, ext);
			break;
		case 403:
			fd = fopen("./forbidden.html", "r");
			stat("./forbidden.html", &statbuf);
			sprintf(saida, "HTTP/1.1 403 Forbidden\r\nDate: %s BRT\r\nServer: Servidor HTTP do Farzin\r\nConnection: %s \r\nContect-Length: %ld \r\nContent-Type: text/html \r\n\r\n", semnewline(asctime(localtime(&tempo))),conexao, (long)statbuf.st_size);
			fprintf(registro, "HTTP/1.1 403 Forbidden\r\nDate: %s BRT\r\nServer: Servidor HTTP do Farzin\r\nConnection: %s \r\nContect-Length: %ld \r\nContent-Type: text/html \r\n\r\n", semnewline(asctime(localtime(&tempo))),conexao, (long)statbuf.st_size);
			break;
		case 404:
			fd = fopen("./notfound.html", "r");
			stat("./notfound.html", &statbuf);
			sprintf(saida, "HTTP/1.1 404 Not Found\r\nDate: %s BRT\r\nServer: Servidor HTTP do Farzin\r\nConnection: %s \r\nContect-Length: %ld \r\nContent-Type: text/html \r\n\r\n", semnewline(asctime(localtime(&tempo))),conexao, (long)statbuf.st_size);
			fprintf(registro, "HTTP/1.1 404 Not Found\r\nDate: %s BRT\r\nServer: Servidor HTTP do Farzin\r\nConnection: %s \r\nContect-Length: %ld \r\nContent-Type: text/html \r\n\r\n", semnewline(asctime(localtime(&tempo))),conexao, (long)statbuf.st_size);
			break;
		case 400:
			fd = fopen("./badrequest.html", "r");
			stat("./badrequest.html", &statbuf);
			sprintf(saida, "HTTP/1.1 400 Bad Request\r\nDate: %s BRT\r\nServer: Servidor HTTP do Farzin\r\nContect-Length: %ld \r\nContent-Type: text/html \r\n\r\n", semnewline(asctime(localtime(&tempo))), (long)statbuf.st_size);
			fprintf(registro, "HTTP/1.1 400 Bad Request\r\nDate: %s BRT\r\nServer: Servidor HTTP do Farzin\r\nContect-Length: %ld \r\nContent-Type: text/html \r\n\r\n", semnewline(asctime(localtime(&tempo))), (long)statbuf.st_size);
			break;
		case 501:
			fd = fopen("./notimplemented.html", "r");
			stat("./notimplemented.html", &statbuf);
			sprintf(saida, "HTTP/1.1 501 Not Implemented\r\nDate: %s BRT\r\nServer: Servidor HTTP do Farzin\r\nContect-Length: %ld \r\nContent-Type: text/html \r\n\r\n", semnewline(asctime(localtime(&tempo))), (long)statbuf.st_size);
			fprintf(registro, "HTTP/1.1 501 Not Implemented\r\nDate: %s BRT\r\nServer: Servidor HTTP do Farzin\r\nContect-Length: %ld \r\nContent-Type: text/html \r\n\r\n", semnewline(asctime(localtime(&tempo))), (long)statbuf.st_size);
			break;
		case 503:
			fd = fopen("./serviceunavailable.html", "r");
			stat("./serviceunavailable.html", &statbuf);
			sprintf(saida, "HTTP/1.1 503 Service Unavailable\r\nDate: %s BRT\r\nServer: Servidor HTTP do Farzin\r\nContect-Length: %ld \r\nContent-Type: text/html \r\n\r\n", semnewline(asctime(localtime(&tempo))), (long)statbuf.st_size);
			fprintf(registro, "HTTP/1.1 503 Not Unavailable\r\nDate: %s BRT\r\nServer: Servidor HTTP do Farzin\r\nContect-Length: %ld \r\nContent-Type: text/html \r\n\r\n", semnewline(asctime(localtime(&tempo))), (long)statbuf.st_size);
			break;
	}

	//Se a requisição é igual a GET, ou o stats é bad request ou not implemented, lê e imprime-se o arquivo solicitado
	if(!strcmp(requisicao, "GET") || status == 400 || status == 501 || status == 503) {
		int i = strlen(saida);
   		 while ((buf = fgetc(fd)) != EOF) {
   		 	saida[i++] = buf;
   		 }
   		 //saida[i]='\0';
   		 //saida[i] = EOF;
	}
	printf("\n");

	return saida;
}

char *semnewline(char *s){ //Função para remover o character '\n' no final da string
	char *aux;
	aux=strdup(s);
	aux[strlen(aux)-1]='\0';
	return aux;
}

char *extensao(char *filename){
  char *aux;
  aux = strrchr(filename, '.');
  return (aux+1);
}

char* concat(char *s1, char *s2){
    char *result = malloc(strlen(s1)+strlen(s2)+1);
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}
