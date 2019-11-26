#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include  <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define FATAL_LEN 30
#define FATAL_STR "An error has occurred\n" 
#define ERROR do { write(STDOUT_FILENO, FATAL_STR, FATAL_LEN); } while(0);

//Exit Codes:
// 0 = All Good;
// 1 = Bad Input;
// 2 = System Error

void myPrint(char *msg)
{
    write(STDOUT_FILENO, msg, strlen(msg));
}

void parse(char *cmd, char **args, int *r_flag, char **red_path);
void exec(char **args, int red, char *red_path);


//built in commands are implemented by me
int main(int argc, char *argv[]) 
{
	char cmd_buff[514];
	char *cmd;
	char *pinput;
	char *save;
	char *red_path;
	int r_flag;

	//That's definitely enough space
	char *args[514];

	while (1) {
		myPrint("myshell> ");
		pinput = fgets(cmd_buff, 514, stdin);
		if (!pinput) {
			ERROR;
			exit(2);
		}

		pinput = strtok(pinput, "\n");

		cmd = strtok_r(pinput, ";", &save);

		while(cmd != NULL){
			//puts cmd into terms exec can run
			parse(cmd, args, &r_flag, &red_path);
			printf("red_=ath: %s\n", red_path);
			printf("R_f;: %d\n", r_flag);
			for(int i = 0; i< 10; i++){
				printf("Arg%d: %s\n", i, args[i]);
				if(args[i]==NULL) break;
			}
			//if no redirect, should return NULL
		//	exec(args, r_flag, red_path);
			cmd = strtok_r(NULL, ";", &save);
		}
    }
}

void parse(char *cmd, char **args, int *r_flag, char **red_path)
{
	//converts input into 2D, null terminiated strong arrays 
	char *path;
	int c = 0;
//	char *save;
	char *red_save;
	*r_flag = 0;

	cmd = strtok_r(cmd, ">", &red_save);
	//puts cmd into terms exec can run
	//if no redirect, should return NULL
	
	*red_path = strtok(NULL, "+");
	*r_flag = 2;
	if(!(*red_path))//if red+path empty
	{
		*red_path = strtok_r(cmd, " ", &red_save);
		if(!(*red_path))*r_flag = 1;
	}
	//if no leading whitespace, gets first arg
	//If nothing but whitespace; returns NULL
	path = strtok(cmd, " "); // get first arg of cmd
	args[c] = path;
	
	//if path is null; first cmd is null
	while(path != NULL){
		args[c++] = path;
		path = strtok(NULL, " ");	
	}	

	args[c++] = NULL; //add terminating char
	
}
void exec(char **args, int red, char *red_path)
{
	pid_t p;
	int stat;
	//by Default, just put it onto STDOUT

	int out_fd = dup(1);
	int fd[2];
	
	if((pipe(fd)) < 0){
		ERROR;
		exit(2);
	}


	
	//Create Fork
	p = fork();
	//Failed Fork
	if (p < 0){
		ERROR;
		exit(2);
	}

	//Succesful Fork; Child
	else if(p == 0){
		dup2(fd[1], STDOUT_FILENO);
		close(fd[0]);
		close(fd[1]);
		if(execvp(args[0], args) < 0){
			ERROR;
			exit(2);
		}
		fflush(stdout);
		//might not need this
		dup2(out_fd, STDOUT_FILENO);
		exit(0);
	}
	
	//Succesful Fork; Parent
	else{
		char buf[1];
		if(red){
			out_fd = open(red_path, O_CREAT | O_WRONLY);	
		}
		while((read(fd[0], buf, 1)) > 0){
			fflush(stdout);
			write(out_fd, buf, 1);
			fflush(stdout);
		}
		close(fd[0]);
		while (waitpid(p, &stat, 0) != p);
	}
}
