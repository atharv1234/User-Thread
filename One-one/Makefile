default:run
run:oneone
lock.o : lock.c lock.h 
			cc -c lock.c
one-one.o : one-one.c lock.h 
			cc -c one-one.c
oneone: one-one.o lock.o 
			cc -o oneone one-one.o lock.o
			
clean:
	rm oneone one-one.o lock.o