	comment "subroutine prologue"
	.macro gdbasm_enter
	push	%ebp
	mov	%esp,%ebp
	.endm

	comment "subroutine epilogue"
	.macro gdbasm_leave
	pop	%ebp
	ret
	.endm

	.macro gdbasm_call subr
	call	\subr
	.endm

	.macro gdbasm_several_nops
	nop
	nop
	nop
	nop
	.endm

	comment "exit (0)"
	.macro gdbasm_exit0
	hlt
	.endm

	comment "crt0 startup"
	.macro gdbasm_startup
	xor	%ebp, %ebp
	.endm

	comment "Declare a subroutine"
	.macro gdbasm_declare name
\name:
	.def \name; .val \name; .scl 2; .type 040; .endef
	.endm

	comment "Declare a data variable"
	.macro gdbasm_datavar name value
	.data
\name:
	.long	\value
	.endm
