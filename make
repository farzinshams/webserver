#! /bin/sh

echo `bison -d parser.y`
echo `flex parser.l`
echo `gcc -g -o servidor servidor.c -ly -lfl -lpthread -w`

