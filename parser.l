%{
#include "parser.tab.h"
#include <string.h>
#include <stdio.h>
%}

%START Seg
%x Dp

comando ^[^ :\r]*
param [^\][^,\r]*
dp [:| ]+
virg [,][ ]?
crlf \r\n

%%

{comando} {
	yylval.string=strdup(yytext);
	BEGIN Dp;
	return COM;
}

<Seg>{param} {
	yylval.string=strdup(yytext);
	return PARAM;
}

<Dp>{dp} {
	BEGIN Seg;
}

{virg} ;


{crlf} {
	BEGIN 0;
	return CRLF;
}
%%