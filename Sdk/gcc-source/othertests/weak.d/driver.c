#include <stdio.h>
extern int strong_data[];
extern int weak_data[];

main()
{
    printf("strong() %d\n", strong()); 
    printf("strong_data[0] %d\n", strong_data[0]);
    printf("\n");

    callweak();

    printf("weak() %d\n", weak()); 
    printf("weak_data[0] %d\n", weak_data[0]);
}
