; Test screen - Draw VGA default palette in VGA 320 x 200, 256 color mode
; Inspired by: https://thiscouldbebetter.wordpress.com/2011/03/17/using-vga-graphics-in-assembly-language/
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

bits 16
org 0x0100

; Constants
NumberOfColors: equ 0x0100
ColorsPerRow: 	equ 0x0012
BoxWidth: 		equ 0x0010
BoxHeight: 		equ 0x000C
BoxesPerRow: 	equ ColorsPerRow * BoxWidth 

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Main:
	; Store current display mode and page
	call DisplayModePageGetOFh
	push ax
	push bx
	; Set VGA mode 0x13, 320 x 200, 256 color
	mov al, 0x13
	call DisplayModeSet00h
	;
	mov ax,0 		; pixel x
	mov bx,0 		; pixel y
	mov cx, NumberOfColors
.DrawEveryColorInPalette:
	;
	push ax			; pixel x
	push bx			; pixel y
	mov dx, NumberOfColors
	sub dx,cx
	push dx			; pixel color index
	call DisplayPixelDrawBoxWidth
	; call DisplayPixelDrawXY	
	;s
	add ax, BoxWidth
	cmp ax, BoxesPerRow
	jb .EndIfNewRowNeeded
		mov ax,0
		add bx, BoxHeight
.EndIfNewRowNeeded:
	; Reminder: loop instruction decreases CX by 1
	loop .DrawEveryColorInPalette
.Exit:
	; Wait for the key press
	call ReadKeyPress00h
	; Reset the display to the original mode
	pop bx
	mov al, bh
	call DisplayActivePageSet05h
	pop ax
	call DisplayModeSet00h
	;
	; End of program
	ret
	;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ReadKeyPress00h:
	; Read key press (blocking)
	; AH = Scan code of the key pressed down
	; AL = ASCII character of the button pressed
	mov ah,0x00
	int 0x16
	ret

DisplayModeSet00h:
	;
	mov ah,0x00
	int 0x10
	;
	ret

DisplayActivePageSet05h:
	; Select active display page
	; AH=05h	AL = Page Number
	mov ah,0x05
	int 0x10
	;
	ret
	
DisplayModePageGetOFh:
	; Get current video mode	AH=0Fh
	; Returns: AL = Video Mode, AH = number of character columns, BH = active page
	mov ah,0x0F ; VGA mode 0x13, 320 x 200, 256 color
	int 0x10
	ret

DisplayPixelDrawXY:
	; (posX, posY, color)
	;
	push bp
	mov bp,sp
	;
	push ax
	push bx
	push cx
	push dx
	;
	mov ah,0x0C 	; for int 0x10: draw a pixel
	mov al,[bp+4] 	; color
	mov bh,0x00 	; page number
	mov cx,[bp+8]	; x pos
	mov dx,[bp+6]	; y pos
	int 0x10		; BIOS interrupt 0x10
	;
	pop dx
	pop cx
	pop bx
	pop ax
	;
	pop bp
	ret 6

DisplayPixelDrawBoxWidth:
	; Draw color box
	; (posX, posY, color)
	push bp
	mov bp,sp
	;
	push ax
	push bx
	push cx
	push dx
	;
	mov cx, BoxWidth
.Loop:
	mov ax, [bp+8]	; x pos
	add ax, cx
	push ax
	mov ax, [bp+6]	; y pos
	push ax
	mov ax, [bp+4]  ; color
	push ax
	call DisplayPixelDrawBoxHeight
	loop .Loop
.EndLoop:
	;
	pop dx
	pop cx
	pop bx
	pop ax
	;
	pop bp
	; Return to calling procedure and pop imm16 bytes from stack
	ret 6 ; 3*arguments of 2 bytes

DisplayPixelDrawBoxHeight:
	; Draw color box
	; (posX, posY, color)
	push bp
	mov bp,sp
	;
	push ax
	push bx
	push cx
	push dx
	;
	mov cx, BoxHeight
.Loop:
	mov ax, [bp+8]	; x pos
	push ax
	mov ax, [bp+6]	; y pos
	add ax, cx
	push ax
	mov ax, [bp+4]  ; color
	push ax
	call DisplayPixelDrawXY
	loop .Loop
.EndLoop:
	;
	pop dx
	pop cx
	pop bx
	pop ax
	;
	pop bp
	; Return to calling procedure and pop imm16 bytes from stack
	ret 6 ; 3*arguments of 2 bytes

PadOutSectorsAllWithZeroes:
    times (0x2000 - ($ - $$)) db 0x00