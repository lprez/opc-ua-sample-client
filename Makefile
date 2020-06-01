CFLAGS=-DPTHREAD
LDFLAGS=-lpthread

%.o: %.c
	gcc -c $< -o $@ $(CFLAGS)

client-tutorial: main.o utils.o
	gcc $(LDFLAGS) -lopen62541 main.o utils.o -o client-tutorial
