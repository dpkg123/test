	.file	"bf-ms-attrib.c"
.global	__fltused
	.stabs	"/dev/fs/M/donn/queued/egcs.source/Tests.packaged/",100,0,0,Ltext0
	.stabs	"bf-ms-attrib.c",100,0,0,Ltext0
	.text
Ltext0:
	.stabs	"gcc2_compiled.",60,0,0,0
	.stabs	"int:t(0,1)=r(0,1);-2147483648;2147483647;",128,0,0,0
	.stabs	"char:t(0,2)=r(0,2);0;127;",128,0,0,0
	.stabs	"long int:t(0,3)=r(0,3);-2147483648;2147483647;",128,0,0,0
	.stabs	"unsigned int:t(0,4)=r(0,4);0000000000000;0037777777777;",128,0,0,0
	.stabs	"long unsigned int:t(0,5)=r(0,5);0000000000000;0037777777777;",128,0,0,0
	.stabs	"long long int:t(0,6)=@s64;r(0,6);01000000000000000000000;0777777777777777777777;",128,0,0,0
	.stabs	"long long unsigned int:t(0,7)=@s64;r(0,7);0000000000000;01777777777777777777777;",128,0,0,0
	.stabs	"short int:t(0,8)=@s16;r(0,8);-32768;32767;",128,0,0,0
	.stabs	"short unsigned int:t(0,9)=@s16;r(0,9);0;65535;",128,0,0,0
	.stabs	"signed char:t(0,10)=@s8;r(0,10);-128;127;",128,0,0,0
	.stabs	"unsigned char:t(0,11)=@s8;r(0,11);0;255;",128,0,0,0
	.stabs	"float:t(0,12)=r(0,1);4;0;",128,0,0,0
	.stabs	"double:t(0,13)=r(0,1);8;0;",128,0,0,0
	.stabs	"long double:t(0,14)=r(0,1);12;0;",128,0,0,0
	.stabs	"complex int:t(0,15)=s8real:(0,1),0,32;imag:(0,1),32,32;;",128,0,0,0
	.stabs	"complex float:t(0,16)=r(0,16);8;0;",128,0,0,0
	.stabs	"complex double:t(0,17)=r(0,17);16;0;",128,0,0,0
	.stabs	"complex long double:t(0,18)=r(0,18);24;0;",128,0,0,0
	.stabs	"__builtin_va_list:t(0,19)=*(0,2)",128,0,0,0
	.stabs	"_Bool:t(0,20)=@s8;-16;",128,0,0,0
	.stabs	"bf-ms-attrib.c",130,0,0,0
	.stabs	"one_gcc:T(1,1)=s8d:(0,1),0,32;a:(0,11),32,8;b:(0,9),40,7;c:(0,2),48,8;;",128,0,0,0
	.stabs	"one_nat:T(1,2)=s12d:(0,1),0,32;a:(0,11),32,8;b:(0,9),48,7;c:(0,2),64,8;;",128,0,0,0
	.def	___main;	.scl	2;	.type	32;	.endef
	.balign 2
	.stabs	"main:F(0,1)",36,0,25,_main
.globl _main
	.def	_main;	.scl	2;	.type	32;	.endef
_main:
.stabn 68,0,25,LM1-_main
LM1:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$8, %esp
	andl	$-16, %esp
	movl	$0, %eax
	movl	%eax, -4(%ebp)
	movl	-4(%ebp), %eax
	call	__alloca
	call	___main
.stabn 68,0,30,LM2-_main
LM2:
	call	_abort
.stabn 68,0,34,LM3-_main
LM3:
Lscope0:
	.stabs	"",36,0,0,Lscope0-_main
	.text
	.stabs "",100,0,0,Letext
Letext:
	.def	_abort;	.scl	2;	.type	32;	.endef
