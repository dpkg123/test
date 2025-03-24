__stdcall foo();
extern bar() __asm__("barbell");

main()
{
	foo();
}
__stdcall foo(){}
bar() {};
