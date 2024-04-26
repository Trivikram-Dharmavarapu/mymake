CC=g++
CFLAG=-Wall -std=c++11 -pedantic
mymake: mymake.o
	$(CC) $(CFLAG) mymake.o -o mymake
mymake.o: mymake.cpp
	$(CC) -c mymake.cpp
clean:
	rm -f a.out
	rm -f *.o
	rm -f *~
	rm -f core.*
	find . -type f -executable -exec rm '{}' \;
