# webserver
HTTP web server
Interesting project when considering what it's built with: C, Bison for syntactical analysis, and Lex for lexical analysis. Does port reading writing, multithreading, http request lexical analysis with lex, syntactical analysis with bison, and saves the request in a multiple chained linked list.

On the command line, type:
$ ./servidor 5544 ./meu-webspace registro.txt

Then, on a web browser, visit:
localhost:5544, or localhost:5544/dir1/texto1.html
