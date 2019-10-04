#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>

// concatenate return value for "Read_string* mum_read();"
typedef struct Read_sstring
{
    char* dirty_input;  // the raw input of user
    char* clean_input;  // the modified input of user
}Read_string;

//  struct for storing Job info
typedef struct Jobb
{
    char* user_input;   // the command given by user
    pid_t pid_number;   // the process id  
}Job;

// process id for all processes in pipe command
pid_t pid_arry[100];
int pid_num = 0;

int error_break = 0;    //  global variable for modifying input
static sigjmp_buf env;  //  local jump for signal processing
int if_bg = 0;          //  check for background process
int job_num = 0;        //  total number of background processes running/ended

//  [ctrl-c] signal handler when user is still inputting
void sighandler1(int signum) {
    signal(SIGINT,sighandler1);
    signum=2;
    siglongjmp(env, 11);
    //exit(1);
}

//  [ctrl-c] signal handler when some processes are running
void sighandler2(int signum) {
    signum=2;
    return;
}


void mum_loop();                                                //  constant loop
Read_string* mum_read();                                        //  read from user input and return concatenated string
char** mum_parse(char* user_input);                             //  parse and return tokens from user input
int mum_execute(char** token,char* user_input, Job* job_arry);  //  executing program (fork here)
int* check_redirection(char** token);                           //  given token, return adequate information regarding pipe/redirection
int redirection(char** token);                                  //  the MAIN function for executing command
int piping(char** token, int total_pipe);                       //  recursively executing pipe commands
int check_duplicate(char** token, int in, int out);             //  error checking for input/output duplication
void print_jobs(Job* job_arry);                                 //  print information of all running/ended background jobs


int main()
{
    mum_loop();
    return EXIT_SUCCESS;
}

//constant loop
void mum_loop() 
{
    int status = 1; 
    Job* job_arry = malloc(sizeof(Job)*100);
    do
    {
        signal(SIGINT,sighandler1);
        fprintf(stdout,"mumsh $ ");
        fflush(stdout);
        if (sigsetjmp(env, 1) == 11)
        {
            fprintf(stdout,"\n");
            fflush(stdout);
            continue;
        }
        Read_string * temp = mum_read();
        char* user_input = temp[0].clean_input;
        char* raw_input = temp[0].dirty_input;
        if (error_break == 1)
        {
            error_break = 0;
            free(user_input);
            free(raw_input);
            free(temp);
            continue;
        }
        if (strlen(user_input)==0)
        {
            free(user_input);
            free(raw_input);
            free(temp);
            break;
        }

        char **token = mum_parse(user_input);
        if (token[0] == NULL)
        {
            free(user_input);
            free(raw_input);
            free(temp);
            free(token);
            continue;
        }
        status = mum_execute(token,raw_input,job_arry);
        free(raw_input);
        free(user_input);
        free(temp);
        free(token);
        error_break = 0;

    } while (status);
    for (int i = 0; i<job_num;i++)
    {
        int status;
        waitpid(job_arry[i].pid_number, &status, 0);
        free(job_arry[i].user_input);
    }
    free(job_arry);
    fprintf(stdout,"exit\n");
    fflush(stdout);
}

void print_jobs(Job* job_arry)
{
    int status;
    for (int i = 0;i<job_num;i++)
    {
        if (waitpid(job_arry[i].pid_number, &status, WNOHANG))
        {
            if (WIFEXITED(status))
                printf("[%d] done %s\n", i + 1, job_arry[i].user_input);
            else
                printf("[%d] running %s\n", i + 1, job_arry[i].user_input);
        }
        else
            printf("[%d] running %s\n", i + 1, job_arry[i].user_input);
    }
}


// return int* redirect_para = {int status, int out_pos, int in_pos, int if_append, int pipe_pos, int num_pipe, int num_out, int num_in}
//             0                    |       1       |     2     |   3                           |       4       |     5      |      6       |      7       |   
//          status                  |   out_pos     |   in_pos  | if_append = 0 if no "<<"      |  first pos of | num of pipe| # of output  |  # of input  |
// 0:   no redirection              |    1025       |   1025    | if_append = 1 if "<<" exists  |   pipe        |            |  redirection |  redirection |
// 1:   only (> or >>)              |   ?<1025      |   1025    |                               |               |            |              |              |
// 2:   only <                      |   1025        |  <1025    |                               |               |            |              |              |
// 3:   both ">/>>" "<" exists,     |  ?smaller     |   bigger  |                               |               |            |              |              |
int* check_redirection(char** token)
{

    int* redirect_para = malloc(sizeof(int)*8);
    int pos = 0;
    int out_pos = 1025;
    int in_pos = 1025;
    int if_append = 0;
    int pipe_pos = -1;
    int if_pipe = 0;
    int num_pipe = 0;
    int num_out = 0;
    int num_in = 0;
    while (token[pos] != NULL)
    {
        if (strcmp(token[pos],"<")==0)
        {
            in_pos = pos;
            num_in++;
        }
        else if (strcmp(token[pos],">")==0)
        {
            out_pos = pos;
            num_out++;
        }
        else if (strcmp(token[pos],">>")==0)
        {
            if_append = 1;
            out_pos = pos;
            num_out++;
        }
        else if (strcmp(token[pos],"|")==0)
        {
            if (if_pipe == 0)
            {
                pipe_pos = pos;
                if_pipe = 1;
            }
            num_pipe++;
        }
        pos++;
    }
    redirect_para[1] = out_pos;
    redirect_para[2] = in_pos;
    redirect_para[3] = if_append;
    redirect_para[4] = pipe_pos;
    redirect_para[5] = num_pipe;
    redirect_para[6] = num_out;
    redirect_para[7] = num_in;
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
    if (token[0] == NULL)
    {
        fprintf(stderr,"error: missing program\n");
        error_break = 1;
        return 0;
    }
    signal(SIGINT,sighandler2);
    int* redirect_para = check_redirection(token);
    int status = redirect_para[0];
    int out_pos = redirect_para[1];
    //printf("%d %d %d",status,out_pos,redirect_para[2]);
    int in_pos = redirect_para[2];
    int out_flags = (redirect_para[3]==0 ? O_RDWR | O_CREAT|O_TRUNC: O_RDWR|O_CREAT|O_APPEND);
    int in_flags = O_RDONLY;

    int loc = 0;
    while (token[loc] != NULL)
    {
        int ssize = strlen(token[loc]);
        if (token[loc][0] == '#')
        {
            if (ssize == 2)
            {
                token[loc][0] = token[loc][1];
                token[loc][1] = '\0';
            }
            else if (ssize == 3)
            {
                token[loc][0] = token[loc][1];
                token[loc][1] = token[loc][2];
                token[loc][2] = '\0';
            }

        }
        for (int i = 0;i<ssize;i++)
        {
            if (token[loc][i] == '@')
                token[loc][i] = ' ';
        }
        loc++;
    }




    if (status == 0) //no redirection, just execute the command
    {
        if (execvp(token[0],token)<0)
            {
                if (strcmp(token[0], "pwd") == 0)
                {
                    char *path = malloc(sizeof(char) * 100);
                    getcwd(path, 100);
                    fprintf(stdout, "%s\n", path);
                    fflush(stdout);
                    free(path);
                    free(redirect_para);
                    return 1;
                }
                fprintf(stderr,"%s: command not found\n",token[0]);
                error_break = 1;
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
            if (status == 1)
                fprintf(stderr,"%s: Permission denied\n",file_name);
            else if (status == 2)
                fprintf(stderr,"%s: No such file or directory\n", file_name);
            error_break = 1;
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
            if (strcmp(token[0], "pwd") == 0)
            {
                char *path = malloc(sizeof(char) * 100);
                getcwd(path, 100);
                fprintf(stdout, "%s\n", path);
                fflush(stdout);
                free(path);
                free(redirect_para);
                return 1;
            }
            fprintf(stderr,"%s: command not found\n",token[0]);
            error_break = 1;
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
            fprintf(stderr,"%s: Permission denied\n",out_file);
            error_break = 1;
            free(token_command);
            free(redirect_para);
            exit(1);
        }
        int fd_in = open(in_file,in_flags, S_IRUSR | S_IWUSR);
        if (fd_in < 0)
        {
            fprintf(stderr,"%s: No such file or directory\n", in_file);
            error_break = 1;
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
            if (strcmp(token[0], "pwd") == 0)
            {
                char *path = malloc(sizeof(char) * 100);
                getcwd(path, 100);
                fprintf(stdout, "%s\n", path);
                fflush(stdout);
                free(path);
                free(redirect_para);
                return 1;
            }
            fprintf(stderr,"%s: command not found\n",token[0]);
            error_break = 1;
            free(token_command);
            free(redirect_para);
            exit(1);
        }
        free(token_command);
        free(redirect_para);
    }
    return 1;
}

int check_duplicate(char** token, int in, int out)
{
    int* redirect_para = check_redirection(token);
    if ((in != 0 && redirect_para[2] < 1025) || (redirect_para[7] > 1))
    {
        fprintf(stderr,"error: duplicated input redirection\n");
        error_break = 1;
        free(redirect_para);
        return 1;
    }
    else if ((out != 1 && redirect_para[1] < 1025) || (redirect_para[6] > 1))
    {
        fprintf(stderr,"error: duplicated output redirection\n");
        error_break = 1;
        free(redirect_para);
        return 1;
    }
    
    free(redirect_para);
    return 0;
    
}

int piping(char** token, int total_pipe)
{
    if (error_break)
        return 1;
    signal(SIGINT,sighandler2);
    int* redirect_para = check_redirection(token);
    int cur_pipe = redirect_para[5];
    int pos = redirect_para[4];
    free(redirect_para);
    if (pos == -1 && total_pipe == 0)
    {
        // int temp = 0;
        // while(token[temp] != NULL)
        // {
        //     printf("temp: %s\n",token[temp]);
        //     fflush(stdout);
        //     temp++;
        // }

        if (check_duplicate(token,0,1) == 1)
            return 1;
        redirection(token);
        return 1;
    }
    else
    {
        char** arg1 = malloc(sizeof(char*)*1024);
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
        arg2 = realloc(arg2,sizeof(char*)*(index-pos+3));
        int fd[2];
        pipe(fd);
        pid_t pid;
        pid = fork();
        if (pid < 0)
        {
            perror("pipe error");
            exit(1);
        }
        else if (pid == 0) //child process [child | parent ]
        {
            dup2(fd[1], 1);
            close(fd[0]);
            if (check_duplicate(arg1,0,fd[1]) == 1)
            {
                free(arg1);
                exit(0);
            }
            redirection(arg1);
            close(1);
            
            exit(0);
        }
        else // parent
        {
            int status;
            pid_arry[pid_num] = pid;
            pid_num++;
            for (int i = 0; i < pid_num; i++)
            {
                waitpid(pid_arry[i], &status, 0);
            }
            
            
            dup2(fd[0],0);
            close(fd[1]);
            if (check_duplicate(arg2,fd[0],1) == 1)
            {
                free(arg1);
                free(arg2);
                return 1;
            }
            if (cur_pipe == 1)
            {
                pid_t temp = fork();
                if (temp < 0)
                {
                    perror("pipe error");
                    exit(1);
                }
                else if (temp == 0)
                {
                    redirection(arg2);
                    exit(0);
                }
                else
                {
                    for (int i = 0; i < pid_num; i++)
                    {
                        waitpid(pid_arry[i], &status, 0);
                    }
                    waitpid(temp,&status,0);
                }
            }
            else
                piping(arg2,total_pipe);

            free(arg1);
            free(arg2);
            return 1;
        }
    }
}

int mum_execute(char** token,char* user_input, Job* job_arry)
{
    if (token[0]==NULL) return 1;
    if (error_break) return 1;
    if (strcmp(token[0],"exit")==0)
    {
        return 0;
    }
    else if(strcmp(token[0],"cd")==0)
    {
        if (token[1] == NULL)
        {
            chdir(getenv("HOME"));
        }
	    else if (chdir(token[1]) != 0)
        {
            fprintf(stderr,"%s: No such file or directory\n",token[1]);
        }
        return 1;
    }
    else if (strcmp(token[0],"jobs")==0)
    {
        print_jobs(job_arry);
        return 1;
    }
    pid_t pid,wpid;
    pid = fork();
    int status;
    if (pid < 0)
    {
        fprintf(stderr,"Fork Error\n");
        exit(1);
    }
    else if (pid == 0) //child process
    {
        signal(SIGINT,sighandler2);
        //signal(SIGINT,SIG_DFL);
        int* redirect_para = check_redirection(token);
        piping(token, redirect_para[5]);
        free(redirect_para);

        exit(0);
    }
    else //parent process
    {
        if (if_bg)
        {
            job_arry[job_num].pid_number = pid;
            job_arry[job_num].user_input = strdup(user_input);
            printf("[%d] %s",job_num+1,user_input);
            job_num++;
            if_bg = 0;
            return 1;
        }
        else
        {
            do
            {
                wpid = waitpid(pid, &status, 0);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
            for (int i = 0; i < pid_num; i++)
            {
                waitpid(pid_arry[i], &status, 0);
            }
        }
    }
    return 1;
}

//reads from standard input
// return a pointer to a strcut named Read_string
//
// typedef struct Read_sstring
// {
//     char* dirty_input;   // the raw input of user
//     char* clean_input;   // the modified input of user
// }Read_string;
Read_string * mum_read() 
{
    //size_t read;
    //char* user_input = NULL;
    int pos = 0;
    //int read_complete = 1;
    int sing = 0;
    int doub = 0;
    int redirect = 0;
    char* user_input = malloc(sizeof(char)*1024);
    char* plain_input = malloc(sizeof(char)*1024);
    Read_string * outputt = malloc(sizeof(Read_string));
    int ppos = 0;
    while (1)
    {
        int c = getchar();
        plain_input[ppos] = c;
        ppos++;
        if (c == EOF)
        {
            user_input[pos] = '\0';
            break;
        }
        if (c == '&')
        {
            if_bg = 1;
            user_input[pos] = c;
            pos++;
            continue;
        }
        if (c == '\n' || c == '\r')
        {
            if (pos >= 1 && user_input[pos-1] == '&')
            {
                user_input[pos-1] = ' ';
                user_input[pos] = c;
                user_input[pos+1] = '\0';
                break;
            }
            if (sing == 0 && doub == 0)
            {
                if (pos >= 1 && !(user_input[pos-1]=='<' || user_input[pos-1]=='>' || user_input[pos-1]=='|'))
                {
                    user_input[pos] = c;
                    user_input[pos+1] = '\0';
                    break;
                }
                else
                {
                    user_input[pos] = c;
                    if (error_break)
                    {
                        plain_input[ppos] = '\0';
                        user_input[pos+1] = '\0';
                        outputt[0].dirty_input = plain_input;
                        outputt[0].clean_input = user_input;
                        return outputt;
                    }
                    pos++;
                    printf("> ");
                    fflush(stdout);
                    continue;
                }
                
            }
            else
            {
                if (sing == 1)  //'<\n => '<!\n'
                {
                    if ((pos >= 2) && (user_input[pos-1]=='<' || user_input[pos-1]=='>' || user_input[pos-1]=='|'))
                    {
                        if (user_input[pos-2] =='\'')
                        {
                            user_input[pos] = '!';
                            user_input[pos+1] = c;
                            pos = pos + 2;
                            printf("> ");
                            fflush(stdout);
                            continue;
                        }
                    }
                }
                else if (doub == 1)     // "<\n => "<!\n
                {
                    if ((pos >= 2) && (user_input[pos-1]=='<' || user_input[pos-1]=='>' || user_input[pos-1]=='|'))
                    {
                        if (user_input[pos-2] =='\"')
                        {
                            user_input[pos] = '!';
                            user_input[pos+1] = c;
                            pos = pos + 2;
                            printf("> ");
                            fflush(stdout);
                            continue;
                        }
                    }
                }
                user_input[pos] = c;
                if (error_break)
                {
                    plain_input[ppos] = '\0';
                    user_input[pos+1] = '\0';
                    outputt[0].dirty_input = plain_input;
                    outputt[0].clean_input = user_input;
                    return outputt;
                }
                pos++;
                printf("> ");
                fflush(stdout);
                continue;
            }
        }

        if (c == '>' || c == '|' || c == '<')
        {
            if (doub == 0 && sing == 0)
            {
                if (pos >= 2)
                {
                    if (user_input[pos - 2] == '>' || user_input[pos - 2] == '|' || user_input[pos - 2] == '<')
                    {
                        if (!(c == '|' && user_input[pos - 2] == '|'))
                        {
                            if (error_break == 0)
                            {
                                fprintf(stderr, "syntax error near unexpected token `%c'\n", c);
                                error_break = 1;
                            }
                        }
                        else
                        {
                            if (error_break == 0)
                            {
                                fprintf(stderr, "error: missing program\n");
                                error_break = 1;
                            }
                        }
                    }
                }
            }
        }
        else if (c == '\'')
        {
            if (doub == 0)
            {
                if (sing == 0)
                {
                    sing = 1;
                }
                else if (sing == 1)
                {
                    if (pos >= 2)        // \n<' => \n'<' 
                    {
                        if (user_input[pos - 2] == '\r' || user_input[pos - 2] == '\n')
                        {
                            if (user_input[pos-1]=='<'||user_input[pos-1]=='>'||user_input[pos-1]=='|')
                            {
                                char temp = user_input[pos-1];
                                user_input[pos-1] = c;
                                user_input[pos] = temp;
                                pos++;
                            }
                        }
                    }
                    sing = 0;
                }
            }
            redirect = 0;
        }
        else if (c == '\"')
        {
            if (sing == 0)
            {
                if (doub == 0)
                {
                    doub = 1;
                }
                else if (doub == 1)
                {
                    if (pos >= 2)
                    {
                        if (user_input[pos - 2] == '\r' || user_input[pos - 2] == '\n')
                        {
                            if (user_input[pos-1]=='<'||user_input[pos-1]=='>'||user_input[pos-1]=='|')
                            {
                                char temp = user_input[pos-1];
                                user_input[pos-1] = c;
                                user_input[pos] = temp;
                                pos++;
                            }
                        }
                    }
                    doub = 0;
                }
            }
            redirect = 0;
        }
        else if (c != ' ' && redirect == 1)
            redirect = 0;
        user_input[pos] = c;
        pos++;
    }
    //getline(&user_input,&read,stdin);
    plain_input[ppos] = '\0';
    outputt[0].dirty_input = plain_input;
    
    size_t length = strlen(user_input);
    if (length==0)
    {
        outputt[0].clean_input = user_input;
        return outputt;
    }
    char* mod_input = malloc(sizeof(char)*(length+100));
    pos = 0;
    unsigned long i = 0;
    while (i < length)
    {
        if (user_input[i] == '\"')
        {
            if (sing == 0)
            {
                if (doub == 0)
                {
                    doub = 1;
                }
                else 
                {
                    doub = 0;
                }
                //mod_input[pos] = user_input[i];   0 1 2
                if (i < length-2)  //'>' => #>
                {
                    if ((user_input[i+1] == '|' || user_input[i+1] == '>' || user_input[i+1] == '<') && (user_input[i+2] =='\"' || user_input[i+2]=='!'))
                    {
                        if (user_input[i+2] == '\"')
                            doub = 0;
                        mod_input[pos] = '#';
                        mod_input[pos+1] = user_input[i+1];
                        pos = pos + 2;
                        i = i+3;
                        continue;
                    }
                }
                if (i < length - 3) // '>>'
                {
                    if (user_input[i+1] == '>' && user_input[i+2] == '>' && (user_input[i+3] == '\"' || user_input[i+3] == '!'))
                    {
                        if (user_input[i+3] == '\"')
                            doub = 0;
                        mod_input[pos] = '#';
                        mod_input[pos+1] = user_input[i+1];
                        mod_input[pos+2] = user_input[i+2];
                        pos = pos + 3;
                        i = i + 4;
                        continue;
                    }
                }
                i++;
                //pos++;
                continue;
            }
        }
        if (user_input[i] == '\'')
        {
            if (doub == 0)
            {
                if (sing == 0)
                {
                    sing = 1;
                }
                else
                {
                    sing = 0;
                }
                //mod_input[pos] = user_input[i];
                if (i < length - 2)
                {
                    if ((user_input[i + 1] == '|' || user_input[i + 1] == '>' || user_input[i + 1] == '<') && (user_input[i + 2] == '\'' || user_input[i+2]=='!'))
                    {
                        if (user_input[i+2] == '\'')
                            sing = 0;
                        mod_input[pos] = '#';
                        mod_input[pos + 1] = user_input[i + 1];
                        pos = pos + 2;
                        i = i + 3;
                        continue;
                    }
                }
                if (i < length - 3)
                {
                    if (user_input[i + 1] == '>' && user_input[i + 2] == '>' && (user_input[i + 3] == '\''|| user_input[i+3] == '!'))
                    {
                        if (user_input[i+3]=='\'')
                            sing = 0;
                        mod_input[pos] = '#';
                        mod_input[pos + 1] = user_input[i + 1];
                        mod_input[pos + 2] = user_input[i + 2];
                        pos = pos + 3;
                        i = i + 4;
                        continue;
                    }
                }
                i++;
                //pos++;
                continue;
            }
        }
        if (doub == 1 || sing == 1)
        {
            if (user_input[i] == ' ')
            {
                mod_input[pos] = '@';
            }
            else
                mod_input[pos] = user_input[i];
            i++;
            pos++;
            continue;
        }

        if (user_input[i] == '<')
        {
            mod_input[pos] = ' ';
            mod_input[pos + 1] = '<';
            mod_input[pos + 2] = ' ';
            pos = pos + 3;
            i++;
        }
        else if (user_input[i] == '>')
        {
            if (i < length-1 && user_input[i + 1] == '>')
            {
                    mod_input[pos] = ' ';
                    mod_input[pos + 1] = '>';
                    mod_input[pos + 2] = '>';
                    mod_input[pos + 3] = ' ';
                    pos = pos + 4;
                    i = i + 2;
            }
            else
            {
                mod_input[pos] = ' ';
                mod_input[pos + 1] = '>';
                mod_input[pos + 2] = ' ';
                pos = pos + 3;
                i++;
            }
            
        }
        else if (user_input[i] == '|')
        {
            mod_input[pos] = ' ';
            mod_input[pos + 1] = '|';
            mod_input[pos + 2] = ' ';
            pos = pos + 3;
            i++;
        }

        else
        {
            mod_input[pos] = user_input[i];
            pos++;
            i++;
        }
    }
    // if (doub == 0 && sing == 0)
    // {
    //     read_complete = 1;
    //     mod_input = realloc(mod_input,pos+5);
    //     mod_input[pos] = '\0';
    // }
    // else
    // {
    //     read_complete = 0;
    //     mod_input = realloc(mod_input,pos+5);
    // }
    mod_input[pos] = '\0';
    free(user_input);
    outputt[0].clean_input = mod_input;
    return outputt;
}

//parse from stdin
char** mum_parse(char* user_input) 
{
    if (user_input == NULL) return NULL;
    int token_size = 100;
    int step_size = 10;
    char* delim = " \t\r\n\a";
    char** token = malloc(sizeof(char*)*token_size);
    int index = 0;
    //int count = 1;
    char* parsing = strtok(user_input,delim);
    //printf("%d: %s: %s\n",count,parsing,user_input);
    do
    {
        if (index >= token_size-1)
        {
            token_size += step_size;
            token = realloc(token,sizeof(char*)*token_size+5);
        }
        token[index] = parsing;
        index++;
        parsing = strtok(NULL, delim);
        

        // count++;
        // printf("%d: %s: %s\n",count,parsing,user_input);
    } while (parsing != NULL);
    token[index] = NULL;
    return token;
}
