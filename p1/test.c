#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char const ** argv)
{
     mum_loop();
}

void mum_loop() //in constant loop
{
    do
    {
        printf("mumsh > ");
        char* stdin = mum_read();
        printf(stdin);
    } while (1);
}

char* mum_read() //reads from standard input
{
    int standard_size = 100;
    int buffer_size = standard_size; //at most 100 characters at first
    char* buffer = malloc(sizeof(char)*buffer_size);
    int index = 0;
    while (1)
    {
        int c = getchar();
        if (index>buffer_size)
        {
            buffer_size += standard_size;
            buffer = realloc(buffer,buffer_size);
        }
        if (c == '\n' || c == EOF)
        {
            buffer[index] = '\0';
            return buffer;
        }
        else
        {
            buffer[index] = c;
            index++;
        }    
    }
}