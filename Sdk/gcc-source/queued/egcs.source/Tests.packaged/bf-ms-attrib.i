# 1 "bf-ms-attrib.c"
# 1 "<built-in>"
# 1 "<command line>"
# 1 "bf-ms-attrib.c"







struct one_gcc {
  int d;
  unsigned char a;
  unsigned short b:7;
  char c;
} __attribute__((__gcc_struct__)) ;


struct one_nat {
  int d;
  unsigned char a;
  unsigned short b:7;
  char c;
} __attribute__((__native_struct__));


main()
  {



    if (sizeof(struct one_nat) != 8)
        abort();
    if (sizeof(struct one_gcc) != 12)
        abort();

  }
