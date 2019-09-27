#include <stdio.h>



struct data{
    int  flag:8;
    int  bit:24;
    
};

int main(){
    struct data d;
    d.flag = 0xf;
    d.bit  = 0xfff;

    printf("size: %d    %X  %X\n",sizeof(d), d.flag, d.bit);
    
}
