CC = gcc
LINKER = gcc

CFLAGS = -std=c99 -Wall

LDFLAGS = -lpthread $(shell pkg-config --libs sdl2)
LDLIBS =  

BUILDDIR= ./build

SRCDIR = ./
SRCS = \
	$(SRCDIR)/main.c \
	$(SRCDIR)/cpu8086.c \
	$(SRCDIR)/cpu8086_instructions.c \
	$(SRCDIR)/bios.c \
	$(SRCDIR)/sdl2_video.c

OBJDIR = $(BUILDDIR)/obj
OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))

EXAMPLES = \
	helloworld.com \
	yourhelp.com \
	palette_vga.com

cpu8086: $(OBJS)
	$(LINKER) -o $(BUILDDIR)/cpu8086 $(OBJS) $(LDFLAGS) $(LDLIBS)

$(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/sdl2_video.o: SDL2 = $(shell pkg-config --cflags sdl2)
$(OBJDIR)/sdl2_video.o: sdl2_video.c sdl2_video.h
	$(CC) $(CFLAGS) $(SDL2) -c sdl2_video.c -o $(OBJDIR)/sdl2_video.o

examples: $(EXAMPLES)

%.com: %.asm
	nasm -fbin -o $(BUILDDIR)/$@ $<

test:
	# gcc -S -std=gnu99 -Os -nostdlib -m16 -march=i386 -ffreestanding test.c
	gcc -S -masm=intel -std=gnu99 -Os -nostdlib -m16 -march=i386 -ffreestanding test.c
	gcc -masm=intel -std=gnu99 -Os -nostdlib -m16 -march=i386 -ffreestanding -o test.com -Wl,--nmagic,--script=com.ld test.c
	sed -i.66 -e "s/\x66//g" test.com
	# gcc -S -masm=intel -std=gnu99 -Os -nostdlib -m32 -march=i386 -ffreestanding test.c
	# nasm -fbin -o test.com test.s

.PHONY : directories
directories:
	mkdir -p $(BUILDDIR)
	mkdir -p $(OBJDIR)

.PHONY : clean
clean:
	$(RM) $(OBJDIR)/*.o
	$(RM) $(BUILDDIR)/*.*
.PHONY : clean

.PHONY : all
all: directories cpu8086 examples 
