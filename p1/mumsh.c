#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void mum_loop();
char* mum_read();
char** mum_parse(char* user_input);
int mum_execute(char** token);
int check_redirection(char* user_input);

int main()
{
     mum_loop();
}

void mum_loop() //in constant loop
{
    do
    {
        printf("mumsh $ ");
        char* user_input = mum_read();
        char** token = mum_parse(user_input);    
        if (token != NULL)
        {
            if (strcmp(token[0],"exit")==0)
                break;
            printf("%d",check_redirection(user_input));
            mum_execute(token);

        }
        free(user_input);
        free(token);
    } while (1);
}

// 0: no redirection
// 1: only >
// 2: only <
// 3: both ">" "<" exists, order: ">" appears first
// 4: both ">" "<" exists, order: "<" appears first
int check_redirection(char* user_input)
{
    char* out = strstr(user_input,">");
    char* in = strstr(user_input,"<");
    if (out == NULL && in == NULL)
        return 0;
    else if (out != NULL && in == NULL)
        return 1;
    else if (out == NULL && in != NULL)
        return 2;
    else
    {
        //char* out_within_in = strstr(in,">");
        char* in_within_out = strstr(out,"<");
        if (in_within_out!=NULL)
            return 3;
        else
            return 4;
    }
}



int mum_execute(char** token)
{
    if (token==NULL) return 1;
    char* command = token[0];
    pid_t pid,wpid;
    pid = fork();
    int status;
    if (pid == -1)
    {
        perror("Fork Error");
        exit(1);
    }
    else if (pid == 0) //child process
    {
        if (execvp(command,token)<0)
        {
            printf("Error: execvp failed!\n");
            exit(1);
        }
        return 1;
    }
    else //parent process
    {
        do
        {
            wpid = waitpid(pid,&status,0);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    return 1;
}

char* mum_read() //reads from standard input
{
    size_t read;
    char* user_input = NULL;
    getline(&user_input,&read,stdin);
    return user_input;
}

char** mum_parse(char* user_input) //parse from stdin
{
    if (user_input == NULL) return NULL;
    int token_size = 100;
    int step_size = 10;
    char* delim = " \n";
    char** token = malloc(sizeof(char*)*token_size);
    int index = 0;
    char* parsing = strtok(user_input,delim);
    do
    {
        if (index >= token_size-1)
        {
            token_size += step_size;
            token = realloc(token,sizeof(char*)*token_size);
        }
        token[index] = parsing;
        index++;
        parsing = strtok(NULL,delim);
    } while (parsing != NULL);
    token[index] = NULL;
    return token;
}

