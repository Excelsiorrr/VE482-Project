#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void mum_loop();
char* mum_read();




int main(int argc, char const ** argv)
{
     mum_loop();
}

void mum_loop() //in constant loop
{
    do
    {
        printf("mumsh > ");
        char* user_input = mum_read();
        printf(user_input);
    } while (1);
}

char* mum_read() //reads from standard input
{
    ssize_t read;
    char* line = NULL;
    getline(&line,&read,stdin);
    return line;
}