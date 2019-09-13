#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void mum_loop();
char* mum_read();
char** mum_parse(char* user_input);
int mum_execute(char** token);
int* check_redirection(char** token);

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
            mum_execute(token);
        }
        free(user_input);
        free(token);
    } while (1);
}


// return int* redirect_para = {int status, int out_pos, int in_pos, int if_append}
    //             0                    |       1       |     2     |   3   |
    //          status                  |   out_pos     |   in_pos  |
    // 0:   no redirection              |    1025       |   1025    |
    // 1:   only (> or >>)              |   ?<1025      |   1025    |
    // 2:   only <                      |   1025        |  <1025    |
    // 3:   both ">/>>" "<" exists,     |  ?smaller     |   bigger  |
    //      order: ">/>>" appears first |               |           |
    // 4:   both ">/>>" "<" exists      |  ?bigger      |   smaller |
    //      order: "<" appears first    |               |           |
int* check_redirection(char** token)
{
    int* redirect_para = malloc(sizeof(int)*3);
    int pos = 0;
    int out_pos = 1025;
    int in_pos = 1025;
    int if_append = 0;
    while (token[pos] != NULL)
    {
        if (strcmp(token[pos],"<")==0)
            in_pos = pos;
        else if (strcmp(token[pos],">")==0)
            out_pos = pos;
        else if (strcmp(token[pos],">>")==0)
        {
            if_append = 1;
            out_pos = pos;
        }
        pos++;
    }
    redirect_para[1] = out_pos;
    redirect_para[2] = in_pos;
    redirect_para[3] = if_append;
    if (out_pos == 1025 && in_pos == 1025)
        redirect_para[0] = 0;
    else if (out_pos != 1025 && in_pos == 1025)
        redirect_para[0] = 1;
    else if (out_pos == 1025 && in_pos != 1025)
        redirect_para[0] = 2;
    else
    {
        if (out_pos < in_pos)
            redirect_para[0] = 3;
        else
            redirect_para[0] = 4;
    }
    return redirect_para;
}

int redirection(char** token, int* redirect_para)
{
    int status = redirect_para[0];
    int out_pos = redirect_para[1];
    //printf("%d %d %d",status,out_pos,redirect_para[2]);
    //int in_pos = redirect_para[2];
    int out_flags = (redirect_para[3]==0 ? O_RDWR | O_CREAT|O_TRUNC: O_RDWR|O_CREAT|O_APPEND);
    if (status == 0) //no redirection, just execute the command
    {
        if (execvp(token[0],token)<0)
        {
            printf("Error: execvp failed!\n");
            exit(1);
        }
    }
    else if (status == 1) //only [command] [> or >>] [filename]
    {
        char** token_command = token;
        token_command[out_pos] = NULL;
        int pos = 0;
        while (token_command[pos]!=NULL)
        {
            printf("%s ",token_command[pos]);
            pos++;
        }

        char* file_name = token[out_pos+1];
        int fd = open(file_name,out_flags, S_IRUSR | S_IWUSR); //everyone can read/write/exeucute.
        if (fd < 0)
        {
            perror("Cannot open\n");
            return -1;
        }
        dup2(fd,1); // redirect stdout to file
        close(fd);
        if (execvp(token_command[0],token_command)<0)
        {
            printf("Error:execvp failed!\n");
            exit(1);
        }
    }
    return 1;
}

int mum_execute(char** token)
{
    if (token==NULL) return 1;
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
        int* redirect_para = check_redirection(token);
        redirection(token,redirect_para);
        free (redirect_para);
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

