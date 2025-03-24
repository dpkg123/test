extern int _weak_data[];

callweak()
{
    printf("from callweak() %d\n", _weak()); 
    printf("from callweak(): _weak_data[0] %d\n", _weak_data[0]);
    printf("\n");
}
