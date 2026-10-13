; Variant-local project CRT. Linear ROM, ZX7, Intel 8080.
; Clear BSS only: DATA is fully overwritten by decompression.
; Inline bit reads; no self-modifying code or ROM writes.

	module clock80_crt0


;--------
; Include zcc_opt.def to find out some info
;--------

        defc    crt0 = 1
        INCLUDE "zcc_opt.def"
        defc CRT_INITIALIZE_BSS = 0

;--------
; Some scope definitions
;--------

        EXTERN    _main           ;main() is always external to crt0 code
        EXTERN    asm_im1_handler

        PUBLIC    __Exit
        PUBLIC    cleanup         ;jp'd to by exit()
        PUBLIC    l_dcal          ;jp(hl)


        defc    TAR__clib_exit_stack_size = 0
        defc    TAR__register_sp = 0xFE00	;-1
	defc    TAR__fputc_cons_generic = 0
	defc	CLIB_DISABLE_FGETS_CURSOR = 1
	defc	CRT_KEY_DEL = 127
	defc	__CPU_CLOCK = 3555555
;	defc	__CPU_CLOCK = 1777000
	
	; RAM trimming
	defc	DEFINED_CLIB_FOPEN_MAX = 1
	defc	CLIB_FOPEN_MAX = 3


        defc CRT_ORG_CODE = 0x0000
        defc    CRT_ORG_DATA = 0x8000
        defc    CRT_ORG_BSS  = 0x8000

        ;defc    CLIB_EXIT_STACK_SIZE = 0


        defc    TAR__crt_enable_rst = $0000

        INCLUDE "crt/classic/crt_rules.inc"
        defc    CONSOLE_COLUMNS = 64
        defc    CONSOLE_ROWS = 25

	PUBLIC	GRAPHICS_CHAR_SET
	PUBLIC	GRAPHICS_CHAR_UNSET
	defc	GRAPHICS_CHAR_SET = 127
	defc	GRAPHICS_CHAR_UNSET = 32


	org	  CRT_ORG_CODE

IF CRT_ORG_CODE = 0x0000

if (ASMPC<>$0000)
        defs    CODE_ALIGNMENT_ERROR
endif

	di
	jp	program
db "(c)Tronix "
;	INCLUDE	"crt/classic/crt_z80_rsts.asm"
ENDIF

program:
        INCLUDE "crt/classic/crt_init_sp.inc"
        EXTERN __BSS_head, __DATA_head
        xra a
        lxi h,__BSS_head
        lxi b,__DATA_head - __BSS_head - 1
        inr b
        inr c
clock_clear_bss:
        mov m,a
        inx h
        dcr c
        jnz clock_clear_bss
        dcr b
        jnz clock_clear_bss
        INCLUDE "crt/classic/crt_init_atexit.inc"
	call    crt0_init
    	;ei
; Optional definition for auto MALLOC init
; it assumes we have free space between the end of
; the compiled program and the stack pointer
IF DEFINED_USING_amalloc
    INCLUDE "crt/classic/crt_init_heap.inc"
ENDIF
	ld	hl,0
	push	hl	;argv
	push	hl	;argc
	call	_main
__Exit:
cleanup:
        di
        jp cleanup ; no monitor ROM on this board

l_dcal: jp      (hl)            ;Used for function pointer calls



; If we were given a model then use it
IF DEFINED_CRT_MODEL
    defc    __crt_model = CRT_MODEL
ELSE
    defc    __crt_model = 2 ; zx7
ENDIF
        defc    __crt_org_bss = CRT_ORG_BSS

	INCLUDE "crt/classic/crt_runtime_selection.inc" 
	
	INCLUDE	"crt/classic/crt_section.inc"
;-------------------------------------------------------------------------
; Native terminal i/o functionality for micro8085_cilb (stdio interface)

        SECTION code_clib

        EXTERN  getk

        PUBLIC  fgetc_cons
        PUBLIC  _fgetc_cons

;-------------------------------------------------------------------------
; Function will stall until data is available to pull from buffer
fgetc_cons:
_fgetc_cons:
        ret

;-------------------------------------------------------------------------
; Native terminal i/o functionality for micro8085_cilb (stdio interface)
; Functions below assumes support for INTEL 8085 "undocumented" opcodes

        SECTION code_clib

        EXTERN  puartd
        EXTERN  puartc

        PUBLIC  fputc_cons_native
        PUBLIC  _fputc_cons_native

;-------------------------------------------------------------------------
; Prints char and inserts CR ahead of LF, i.e. printf("\n") becomes "\r\n"
fputc_cons_native:
_fputc_cons_native:
        ret

IF ( __crt_model = 2)
	SECTION bss_clib

	PUBLIC zaloha 

	zaloha:
  	defb   0

        SECTION code_clib
	SECTION code_compress_zx7

	PUBLIC asm_dzx7_standard
	EXTERN zaloha

asm_dzx7_standard:

;-----------------------------------------------------------------------------
; ZX7 decoder by Einar Saukas
; "Standard" version
; i8080 port Roman Borik (108 bytov)
;-----------------------------------------------------------------------------

;-----------------------------------------------------------------------------
; Parameters:
;   HL: source address (compressed data)
;   DE: destination address (decompressing)
;------------------------------------------------------------------------------
; ZX7: A holds the bit reservoir for literals, E for gamma/offset bits.
; Save it in BSS only across match copying. No self-modifying code.
dzx7:           mvi a,80h
copy_byte_loop: push psw
                mov a,m
                stax d
                inx h
                inx d
                pop psw
main_loop:
                add a
                jnz literal_bit
                mov a,m
                inx h
                ral
literal_bit:    jnc copy_byte_loop
                push d
                mov e,a
                lxi b,0
                mov d,b
length_size_loop:
                inr d
                mov a,e
                add a
                mov e,a
                jnz clock_bit_0
                mov a,m
                inx h
                ral
                mov e,a
clock_bit_0:
                jnc length_size_loop
length_value_loop:
                jc length_have_bit
                mov a,e
                add a
                mov e,a
                jnz clock_bit_1
                mov a,m
                inx h
                ral
                mov e,a
clock_bit_1:
length_have_bit:
                mov a,c
                ral
                mov c,a
                mov a,b
                ral
                mov b,a
                jc clock_zx7_end
                dcr d
                jnz length_value_loop
                inx b
                mov a,e
                sta zaloha
                mov a,m
                inx h
                stc
                ral
                mov e,a
                jnc short_offset
                push d
                lda zaloha
                mov e,a
                mvi d,0e0h
rld_next_bit:
                mov a,e
                add a
                mov e,a
                jnz clock_bit_2
                mov a,m
                inx h
                ral
                mov e,a
clock_bit_2:
                mov a,d
                ral
                mov d,a
                jc rld_next_bit
                mov a,e
                sta zaloha
                inr d
                mov a,d
                rar
                mov d,a
                xthl
                mov a,l
                rar
                mov e,a
                pop h
                jmp offset_ready
short_offset:
                mov a,e
                rar
                mov e,a
offset_ready:
                xthl
                push h
                mov a,l
                sbb e
                mov l,a
                mov a,h
                sbb d
                mov h,a
                pop d
                dcx b
                inr b
                inr c
copy_loop:      mov a,m
                stax d
                inx h
                inx d
                dcr c
                jnz copy_loop
                dcr b
                jnz copy_loop
                pop h
                lda zaloha
                jmp main_loop
clock_zx7_end:  pop h
                ret

ENDIF