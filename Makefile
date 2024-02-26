CC = gcc

CFLAGS = -std=c99 -Wall

LDFLAGS =  
OBJS = 
			
cpu8086: cpu8086.c cpu8086.h $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o cpu8086 cpu8086.c $(LDFLAGS)

helloworld: helloworld.asm
	nasm -fbin -o helloworld.com helloworld.asm

yourhelp: yourhelp.asm
	nasm -fbin -o yourhelp.com yourhelp.asm

clean:
	$(RM) *.o
	$(RM) cpu8086 helloworld.com yourhelp.com
