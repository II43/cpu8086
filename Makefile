CC = gcc
LINKER = gcc

CFLAGS = -std=c99 -Wall

LDFLAGS = 
LDLIBS =  
OBJS = 
			
cpu8086: cpu8086.c cpu8086.h cpu8086_instructions.c cpu8086_instructions.h $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -c main.c  
	$(CC) $(CFLAGS) $(OBJS) -c cpu8086.c
	$(CC) $(CFLAGS) $(OBJS) -c cpu8086_instructions.c
	$(CC) $(CFLAGS) $(OBJS) -c bios.c
	$(LINKER) -o cpu8086 main.o cpu8086.o cpu8086_instructions.o bios.o $(LDFLAGS) $(LDLIBS)

helloworld: helloworld.asm
	nasm -fbin -o helloworld.com helloworld.asm

yourhelp: yourhelp.asm
	nasm -fbin -o yourhelp.com yourhelp.asm

palette_vga: palette_vga.asm
	nasm -fbin -o palette_vga.com palette_vga.asm

test:
	# gcc -S -std=gnu99 -Os -nostdlib -m16 -march=i386 -ffreestanding test.c
	gcc -S -masm=intel -std=gnu99 -Os -nostdlib -m16 -march=i386 -ffreestanding test.c
	gcc -masm=intel -std=gnu99 -Os -nostdlib -m16 -march=i386 -ffreestanding -o test.com -Wl,--nmagic,--script=com.ld test.c
	sed -i.66 -e "s/\x66//g" test.com
	# gcc -S -masm=intel -std=gnu99 -Os -nostdlib -m32 -march=i386 -ffreestanding test.c
	# nasm -fbin -o test.com test.s

clean:
	$(RM) *.o
	$(RM) cpu8086 helloworld.com yourhelp.com palette_vga.com test.com* test.s
