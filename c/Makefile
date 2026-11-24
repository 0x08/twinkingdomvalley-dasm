CFLAGS =

playtkv : playtkv.o libtkv.o pictures.o m6502.o
	gcc -o playtkv playtkv.o libtkv.o pictures.o m6502.o -lm

runtkv : runtkv.o libtkv.o pictures.o m6502.o
	gcc -o runtkv runtkv.o libtkv.o pictures.o m6502.o -lm

readtkv : readtkv.o libtkv.o pictures.o m6502.o
	gcc -o readtkv readtkv.o libtkv.o pictures.o m6502.o -lm

readtkv.o : readtkv.c libtkv.h pictures.h
	gcc $(CFLAGS) -c readtkv.c

playtkv.o : playtkv.c libtkv.h pictures.h
	gcc $(CFLAGS) -c playtkv.c

runtkv.o : runtkv.c libtkv.h m6502.h pictures.h
	gcc $(CFLAGS) -c runtkv.c

libtkv.o : libtkv.c libtkv.h
	gcc $(CFLAGS) -c libtkv.c

pictures.o : pictures.c pictures.h m6502.h
	gcc $(CFLAGS) -O2 -c pictures.c

m6502.o : m6502.c m6502.h
	gcc $(CFLAGS) -O2 -c m6502.c

clean :
	rm -f readtkv playtkv runtkv *.o

dotests: readtkv
	./readtkv dotests

test:
	sh ../tests/run_tests.sh
