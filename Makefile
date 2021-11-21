driver.o: driver.c scanner.h
	gcc -Wall -g -c driver.c
scanner.o: scanner.c scanner.h
	gcc -Wall -g -c scanner.c
parser.o: parser.c scanner.h ast.h
	gcc -Wall -g -c parser.c
ast-print.o: ast-print.c ast.h
	gcc -Wall -g -c ast-print.c
compile: driver.o scanner.o parser.o ast-print.o
	gcc -g driver.o scanner.o parser.o ast-print.o -o compile

.PHONY:	clean
clean:
	rm *.o compile