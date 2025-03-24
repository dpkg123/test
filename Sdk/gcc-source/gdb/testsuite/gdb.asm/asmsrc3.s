	.include "common.inc"
	.include "arch.inc"
	comment "empty file, to provide EOF entry in stabs"

comment        "This file is not linked with crt0."
comment        "Provide very simplistic equivalent."

	.global _start
gdbasm_declare _start
	gdbasm_startup
	gdbasm_call main
	gdbasm_exit0

comment "The next nonblank line must be 'offscreen' w.r.t. start"
comment "and it must not be the last line in the file (I don't know why)"



comment "Provide something to search for"

