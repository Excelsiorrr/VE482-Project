#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

void mum_loop();
char* mum_read();
char** mum_parse(char* user_input);
int mum_execute(char** token);
int* check_redirection(char** token);
int piping(char** token);

int main()
{
     mum_loop();
     return EXIT_SUCCESS;
}

void mum_loop() //in constant loop
{
    int status;
    do
    {
        fprintf(stdout,"mumsh $ ");
        fflush(stdout);
        char* user_input = mum_read();
        char** token = mum_parse(user_input); 
        status=mum_execute(token);
        free(user_input);
        free(token);
    } while (status);
}


// return int* redirect_para = {int status, int out_pos, int in_pos, int if_append, int pipe_pos}
    //             0                    |       1       |     2     |   3                           |       4       |
    //          status                  |   out_pos     |   in_pos  | if_append = 0 if no "<<"      |  first pos of pipe
    // 0:   no redirection              |    1025       |   1025    | if_append = 1 if "<<" exists  |
    // 1:   only (> or >>)              |   ?<1025      |   1025    |                               |
    // 2:   only <                      |   1025        |  <1025    |                               |
    // 3:   both ">/>>" "<" exists,     |  ?smaller     |   bigger  |                               |
int* check_redirection(char** token)
{
    int* redirect_para = malloc(sizeof(int)*5);
    int pos = 0;
    int out_pos = 1025;
    int in_pos = 1025;
    int if_append = 0;
    int pipe_pos = -1;
    int if_pipe = 0;
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
        else if (strcmp(token[pos],"|")==0 && if_pipe==0)
        {
            pipe_pos = pos;
            if_pipe = 1;
        }
        pos++;
    }
    redirect_para[1] = out_pos;
    redirect_para[2] = in_pos;
    redirect_para[3] = if_append;
    redirect_para[4] = pipe_pos;
    if (out_pos == 1025 && in_pos == 1025)
        redirect_para[0] = 0;
    else if (out_pos != 1025 && in_pos == 1025)
        redirect_para[0] = 1;
    else if (out_pos == 1025 && in_pos != 1025)
        redirect_para[0] = 2;
    else
        redirect_para[0] = 3;
    return redirect_para;
}




// perform single command even if no redirections exist
int redirection(char** token)
{
    int* redirect_para = check_redirection(token);
    int status = redirect_para[0];
    int out_pos = redirect_para[1];
    //printf("%d %d %d",status,out_pos,redirect_para[2]);
    int in_pos = redirect_para[2];
    int out_flags = (redirect_para[3]==0 ? O_RDWR | O_CREAT|O_TRUNC: O_RDWR|O_CREAT|O_APPEND);
    int in_flags = O_RDONLY;
    if (status == 0) //no redirection, just execute the command
    {
        if (execvp(token[0],token)<0)
            {
                fprintf(stdout,"Error: execvp failed!\n");
                fflush(stdout);
                free(redirect_para);
                exit(1);   
            }
        
        free(redirect_para);
    }
    else if (status == 1 || status == 2) //only [command] [> or >>] [filename]
    {
        int pos = (status==1 ? out_pos : in_pos);
        int flags = (status==1 ? out_flags : in_flags);
        char** token_command = token;
        char* file_name = token[pos+1];

        int ppos = pos+2;
        while (token_command[ppos] != NULL)
        {
            token_command[pos] = token_command[ppos];
            pos++;
            ppos++;
        }
        token_command[pos] = NULL;
        int fd = open(file_name,flags, S_IRUSR | S_IWUSR); //everyone can read/write/exeucute.
        if (fd < 0)
        {
            fprintf(stdout,"Cannot open %s!\n",file_name);
            fflush(stdout);
            free(token_command);
            free(redirect_para);
            exit(1);
        }
        if (status == 1)
            dup2(fd,1); // redirect stdout to file
        else
            dup2(fd,0); // redirect file to stdin
        close(fd);
        if (execvp(token_command[0],token_command)<0)
        {            
            fprintf(stdout,"Error:execvp failed!\n");
            fflush(stdout);
            free(token_command);
            free(redirect_para);
            exit(1);
        }
        free(token_command);
        free(redirect_para);
    }
    else//[command] [> or >>] [filename1] [filename2] [<] [filename3]
    {
        char** token_command = malloc(sizeof(char*)*1024);
        int index = 0; // count for new command
        int track = 0; // track index for original command
        while (token[track] != NULL)
        {
            if (track == out_pos || track == (in_pos))
            {
                track = track + 2;
            }
            else
            {
                token_command[index] = token[track];
                track++;
                index++;
            }
        }
        token_command[index] = NULL;
        char* in_file = token[in_pos+1];
        char* out_file = token[out_pos+1];
        int fd_out = open(out_file,out_flags, S_IRUSR | S_IWUSR);
        if (fd_out < 0)
        {
            fprintf(stdout,"Cannot open file %s\n",out_file);
            fflush(stdout);
            free(token_command);
            free(redirect_para);
            exit(1);
        }
        int fd_in = open(in_file,in_flags, S_IRUSR | S_IWUSR);
        if (fd_in < 0)
        {
            fprintf(stdout,"Cannot open file %s!\n", in_file);
            fflush(stdout);
            free(token_command);
            free(redirect_para);
            exit(1);
        }
        dup2(fd_in,0);
        close(fd_in);
        dup2(fd_out,1);
        close(fd_out);
        if (execvp(token_command[0],token_command)<0)
        {
            fprintf(stdout,"Error: execvp failed!\n");
            fflush(stdout);
            free(token_command);
            free(redirect_para);
            exit(1);
        }
        free(token_command);
        free(redirect_para);
    }
    return 1;
}

int piping(char** token)
{
    int* redirect_para = check_redirection(token);
    int pos = redirect_para[4];
    if (pos == -1)
    {
        redirection(token);
    }
    else
    {
        char** arg1 = malloc(sizeof(char*)*(pos+1));
        char** arg2 = malloc(sizeof(char*)*1024);
        int index = 0;
        while (token[index]!=NULL)
        {
            if (index < pos)
                arg1[index] = token[index];
            else if (index > pos)
                arg2[index-pos-1] = token[index];
            index++;
        }
        arg1[pos] = NULL;
        arg2[index-pos-1] = NULL;
        arg2 = realloc(arg2,sizeof(char*)*(index-pos));
        int fd[2];
        pipe(fd);
        pid_t pid ,wpid;
        pid = fork();
        int status;
        if (pid < 0)
        {
            fprintf(stdout,"Fork Error\n");
            fflush(stdout);
            exit(1);
        }
        else if (pid == 0) //child process
        {
            close(1);
            close(fd[0]);
            dup2(fd[1],1);
            piping(arg1);
        }
        else
        {
            close(0);
            close(fd[1]);
            dup2(fd[0], 0);
            piping(arg2);
            do
            {
                wpid = waitpid(pid,&status,0);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        }
        
    }
    free(redirect_para);
    return 1;
}

int mum_execute(char** token)
{
    if (token==NULL) return 1;
    if (strcmp(token[0],"exit")==0)
    {
        fprintf(stdout,"exit");
        fflush(stdout);
        exit(0);
    }
    else if(strcmp(token[0],"pwd")==0)
    {
    	char *path = malloc(sizeof(char)*100);
        getcwd(path,100);
        fprintf(stdout,"%s\n",path);
        fflush(stdout);
        free(path);
        return 1;
	}
    else if(strcmp(token[0],"cd")==0)
    {
	    if (chdir(token[1]) != 0)
        {
            fprintf(stdout,"chdir error\n");
            fflush(stdout);
        }
        return 1;
    }
            
    pid_t pid,wpid;
    pid = fork();
    int status;
    if (pid < 0)
    {
        fprintf(stdout,"Fork Error\n");
        fflush(stdout);
        exit(1);
    }
    else if (pid == 0) //child process
    {
        piping(token);
        fprintf(stdout,"finished%d\n",pid);
        fflush(stdout);
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
    size_t length = strlen(user_input);
    char* mod_input = malloc(sizeof(char)*(length+10));
    int pos = 0;
    unsigned long i = 0;
    while (i < length)
    {
        if (user_input[i] == '<')
        {
            mod_input[pos] = ' ';
            mod_input[pos+1] = '<';
            mod_input[pos+2] = ' ';
            pos = pos + 3;
            i++;
        }
        else if (user_input[i] == '>')
        {
            if (user_input[i+1] == '>')
            {
                mod_input[pos] = ' ';
                mod_input[pos+1] = '>';
                mod_input[pos+2] = '>';
                mod_input[pos+3] = ' ';
                pos = pos + 4;
                i = i+2;
            }
            else
            {
                mod_input[pos] = ' ';
                mod_input[pos+1] = '>';
                mod_input[pos+2] = ' ';
                pos = pos + 3;
                i++;
            }
        }
        else if (user_input[i] == '|')
        {
            mod_input[pos] = ' ';
            mod_input[pos+1] = '|';
            mod_input[pos+2] = ' ';
            pos = pos + 3;
            i++;
        }
        else
        {
            mod_input[pos] = user_input[i];
            pos++;i++;
        }
        
    }
    mod_input[pos] = '\0';
    free(user_input);
    return mod_input;
}

char** mum_parse(char* user_input) //parse from stdin
{
    if (user_input == NULL) return NULL;
    int token_size = 100;
    int step_size = 10;
    char* delim = " \t\r\n\a";
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

