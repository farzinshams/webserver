%{
#include<stdio.h>
#include<stdlib.h>
#include<string.h>


struct comando { //Struct do comando
  char *nomecomando;
  struct comando *next;
  struct parametro *nextp;
};
struct parametro { //Struct dos parâmetros/opções
	char *nomeparametro;
	struct parametro *next;
};

struct comando *yyerror();

%}
%union {
        char *string;
}
%token <string> COM
%token <string> PARAM
%token CRLF
%parse-param {struct comando **c} {struct parametro **p}
%%
linhaf: linhas CRLF 
      ;

linhas: linha
	  | linhas linha
	  ;

linha: COM par CRLF {
		//Neste caso, o comando corrente receberá, em seu apontador para parâmetro, o valor da variável "p",
		//isto é, o endereço da lista de parâmetros que estava sendo criada

		if(*c==NULL){//Se este for o primeiro comando, cria-se a lista de comandos
			*c=(struct comando *)malloc(sizeof(struct comando));
			(*c)->nomecomando=strdup($1);
			(*c)->nextp = *p;
			(*c)->next=NULL;
		} else {//Caso contrário, vai-se até o final da lista e cria-se o nó
			struct comando *aux=*c;
			while(aux->next != NULL){
				aux=aux->next;
			}
			aux->next = (struct comando *)malloc(sizeof(struct comando));
			aux=aux->next;
			aux->nomecomando=strdup($1);
			aux->nextp = *p;
			aux->next=NULL;
		}
	}

	 | COM CRLF {
	 		//Neste caso, não há parâmetros associados a este comando. Então, ao criar-se o nó
	 		//deste comando, o apontador ao parâmetro recebe null

	 	if(*c==NULL){//Se este for o primeiro comando, cria-se a lista
			*c=(struct comando *)malloc(sizeof(struct comando));
			(*c)->nomecomando=strdup($1);
			(*c)->nextp = NULL;
			(*c)->next=NULL;
		} else { //Se este não for o primeiro comando, percorre-se a lista até o final e cria-se outro
				 //struct
			struct comando *aux=*c;
			while(aux->next != NULL){
				aux=aux->next;
			}
			aux->next = (struct comando *)malloc(sizeof(struct comando));
			aux=aux->next;
			aux->nomecomando=strdup($1);
			aux->nextp = NULL;
			aux->next=NULL;
		}
	 }
	 ;

par: par PARAM {
		//Este é o caso para o segundo parâmetro ou adiante. Deve-se percorrer a lista dos parâmetros
		//até encontrar-se o último nó. Feito isso, aloca-se mais um struct.
		struct parametro *aux = *p;
		while(aux->next != NULL){
			aux=aux->next;
		}
		aux->next = (struct parametro *)malloc(sizeof(struct parametro));
		aux=aux->next;
		aux->nomeparametro=strdup($2);
		aux->next=NULL;
	}

 	| PARAM {
 		//Este caso é para o primeiro parâmetro: deve-se alocar um struct
 		*p=(struct parametro *)malloc(sizeof(struct parametro)); 
 		(*p)->nomeparametro=strdup($1);
 		(*p)->next=NULL;
 	}
 	;

%%

struct comando *yyparser(){
    struct comando *c=NULL; //Variável global utilizada para referenciar-se as listas dos comandos
    struct parametro *p; //Variável global utilizada para referenciar-se as listas dos parâmetros
    yyparse(&c, &p);
    return c;
}

struct comando *yyerror (){
	struct comando *c=(struct comando *)malloc(sizeof(struct comando));
	c->nomecomando = strdup("erro");
	return c;
}

void imprimir(struct comando *c, FILE *registro){
    //O trecho a seguir percorre a lista de comandos e de parâmetros, imprimindo-se tudo em stdout e registro.txt
	struct comando *cc=c;
	if(!strcmp(cc->nomecomando, "erro")){
		printf("\n400 Bad Request\n\n");
		return;
	}
	printf("%s ", cc->nomecomando);
	fprintf(registro, "%s ", cc->nomecomando);
    while (1){ 
    	struct parametro *aux = cc->nextp;
    	while(aux!=NULL){
	    	if(aux->next != NULL) {printf("%s, ", aux->nomeparametro); fprintf(registro, "%s ", aux->nomeparametro);}
	    	else {printf("%s \r\n", aux->nomeparametro); fprintf(registro, "%s \r\n", aux->nomeparametro);}
	    	aux=aux->next;
	    }
		cc=cc->next;
		if(cc==NULL) break;
	    printf("%s: ", cc->nomecomando);
	    fprintf(registro, "%s: ", cc->nomecomando);
    }
    printf("\r\n");
    fprintf(registro, "\r\n");
}


