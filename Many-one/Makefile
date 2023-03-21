default:run
run:manyone
func.o : func.c mlock.h func.h
			cc -c func.c
mlock.o : mlock.c mlock.h func.h
			cc -c mlock.c
many-one.o : many-one.c mlock.h func.h
			cc -c many-one.c
manyone : many-one.o mlock.o func.o
			cc -o manyone many-one.o mlock.o func.o
			
clean:
	rm many-one.o mlock.o func.o manyone