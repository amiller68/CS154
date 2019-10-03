cc = gcc
CFLAGS = -Wall
OBJS = bar.o foo.o
RM = rm

main: $(OBJS)
	$(cc) $(CFLAGS) $(OBJS) -o main

bar.o: bar.c bar.h
	$(cc) $(CFLAGS) -c bar.c -o bar.o

foo.o: foo.c foo.h bar.h
	$(cc) $(CFLAGS) -c foo.c -o foo.o

clean:
	$(RM) -f $(OBJS) main


