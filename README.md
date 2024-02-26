# cpu8086 - Simple emulator of x86 (16bit)
## Introduction
This is yet another implementation of **basic and in-complete emulator of x86** developed purely for **learning purposes** in C language.
It focuses on 16-bit instruction set and operation. Basically targeting emulation of Intel 8086, 80186 or 80286 and its clones. 
If you need serious x86 emulator , please look at [DOSBox](https://www.dosbox.com/) or others. 

This project is being executed in stages:
### Stage 1
<u>Goal</u>: Emulator is able to execute a very simple "Hello World" or similar binary.

### Stage 2
<u>Goal</u>: Emulator is able to work with VGA graphics. Ideally running some very simple game. 

All this is running under Linux. I am using openSUSE Leap 15.5 but it shall work in any distribution really.
## References
References are typically at the end but in this case I would like to start with them as I took inspiration in all of these. 
### Instructions set of x86
* [80x86 Instructions by Opcode](http://www.c-jump.com/CIS77/reference/Instructions_by_Opcode.html)<br>
This allows to find instruction by opcode.
* [x86 Instruction Set Reference](https://c9x.me/x86/)><br>
This gives more details about each assembly instruction.
* [x86 and amd64 instruction reference](https://www.felixcloutier.com/x86/) by Félix Cloutier<br>
This also gives more details about instructions but sometimes it helps for better understanding. 
### Interrups and MS-DOS function calls
These provide overview about main MS-DOS API under given interrupt vector:
* [INT 21h](https://www.philadelphia.edu.jo/academics/qhamarsheh/uploads/Lecture%2021%20MS-DOS%20Function%20Calls%20_INT%2021h_.pdf) (or [this](http://bbc.nvg.org/doc/Master%20512%20Technical%20Guide/m512techb_int21.htm))
* [INT 10h](https://en.wikipedia.org/wiki/INT_10H)
* [Register values at .COM file startup](http://www.fysnet.net/yourhelp.htm)
### Other emulators
* [8086 - Simple 8086 emulator](https://github.com/reymontero/8086) in C
* [Very downstripped 8086 emulator](https://github.com/julienaubert/py8086) in Python
* [Faux86](https://github.com/jhhoward/Faux86) in C++<br>
A portable, open-source 8086 PC emulator for bare metal Raspberry Pi.
### Other
* [Arithmetic and Logic Instructions](https://www.ic.unicamp.br/~celio/mc404-2006/flags.html)<br>
This includes overview of status (or flags) register.
* [ModR/M addressing microcode in the Intel 8086 processor](http://www.righto.com/2023/02/8086-modrm-addressing.html)<br>
This helps to understand the ModR/M byte.
* [Základy assembleru - Online kurz](https://www.itnetwork.cz/assembler/zaklady)<br>
Basics of assembler online course but this is in czech language only! 
## Stage 1
Not implemented yet. 

## Stage 2
Not implemented yet. 