#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include  <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

void ERROR(){
	char error_message[30] = "An error has occurred\n";
	write(STDOUT_FILENO, error_message, strlen(error_message));
}

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

	if(argc > 2){ERROR(); exit(2); }
	char cmd_buff[513];
	char *cmd;
	char *pinput;
	char *save;
	char *red_path;
	int r_flag;

	//That's definitely enough space
	char *args[514];
	
	FILE *batch = NULL;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	
	if (argc == 2) {
		batch = fopen(argv[1], "r");
		if(!batch) exit(2);
	}
	
	while (1) {
		
		if(batch){
			if((read = getline(&line, &len, batch)) == -1) break;
			strncpy(cmd_buff, line, 512);
			if(len > 512){
				cmd_buff[512] = '\0'; //null terminating if none
				ERROR();
			}
			else {
				//Null terminate at newline
				char *cut = strchr(cmd_buff, '\n');
				*cut = '\0';
			}
			
			myPrint(cmd_buff); myPrint("\n");
		}
		
		else{
			myPrint("myshell> ");
			pinput = fgets(cmd_buff, 513, stdin);
			if (!pinput) {
				ERROR();
				exit(2);
			}
			char *cut = strchr(cmd_buff, '\n');
			*cut = '\0';
		}

		pinput = cmd_buff;

		cmd = strtok_r(pinput, ";", &save);

		while(cmd != NULL){

			//puts cmd into terms exec can run
			parse(cmd, args, &r_flag, &red_path);
			//if no redirect, should return NULL

			if (strcmp(args[0],"exit") == 0) {
				if( args[1] || r_flag ) {
					ERROR();
				}
				else {
					exit(0);
				}
			}

			else if (strcmp(args[0], "pwd") == 0) {
				if( args[1] || r_flag ) {
					ERROR();
				}
				else {
					char cwd_buff[PATH_MAX];
					myPrint(getcwd(cwd_buff, sizeof(cwd_buff))); myPrint("\n");	
				}
			}

			else if (strcmp(args[0], "cd") == 0) {
				if( args[2] || r_flag ){
					ERROR();
				}

				else {
				// need to implement null args 1 for home
					if( args[1] ) chdir(args[1]);
					else chdir(getenv("HOME"));
				}
			}

			else {
				exec(args, r_flag, red_path);
			}
			cmd = strtok_r(NULL, ";", &save);
		}
    }
	free(line);
}

void parse(char *cmd, char **args, int *r_flag, char **red_path)
{
	//converts input into 2D, null terminiated sting array
	// parses redirection as well 
	char *path;
	int c = 0;
	*r_flag = 0;
	
	if((*red_path = strstr(cmd, ">+")))
	{
		*red_path = strtok(*red_path, ">");
		*red_path = strtok(*red_path, "+");
		*red_path = strtok(*red_path, " ");
		if((strtok(NULL, " "))) ERROR(); //accounts for extra words
		*r_flag = 2;
	}
	
	//find redirection in line
	else if( (*red_path = strstr(cmd, ">")) ) //if redi exists
	{
		*red_path = strtok(*red_path, ">");
		*red_path = strtok(*red_path, " ");
		*r_flag = 1;
		if((strtok(NULL, " "))) ERROR(); //accounts for extra words
	}

	cmd = strtok(cmd, ">");
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


void exec(char **args, int r_flag, char *red_path)
{


	//dont execute null arguments
	if(*args == NULL) exit(0);
	
	pid_t p;
	int stat;

	//by Default, just put it onto STDOUT
	int out_fd = dup(1);
	int fd[2];
	
	if((pipe(fd)) < 0){
		ERROR();
		exit(2);
	}
	
	//Create Fork
	p = fork();
	//Failed Fork
	if (p < 0){
		ERROR();
		exit(2);
	}

	//Succesful Fork; Child
	else if(p == 0){
		close(fd[0]);
		dup2(fd[1], STDOUT_FILENO);
		close(fd[1]);
		if(execvp(args[0], args) < 0){
			ERROR();
			exit(2);
		}
		//Once done, make sure STDOUT returned to normal
		dup2(out_fd, STDOUT_FILENO);
		exit(0);
	}
	
	//Succesful Fork; Parent
	else{
		close(fd[1]);
		char buf[1];
		int temp;
		if(r_flag == 1){
			if(red_path && access(red_path, F_OK)){
				out_fd = open(red_path, O_CREAT | O_WRONLY, 0664);
			}
			else {
				ERROR();
				goto end;
			}	
		}

		if(r_flag == 2){
			if(red_path){	
				temp = open("tmp", O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
				if(access(red_path, F_OK)){ 
					//If not found make a new file
					out_fd = open(red_path, O_CREAT | O_WRONLY, 0664);
				}
				else {
					out_fd = open(red_path, O_RDONLY);
					while((read(out_fd, buf, 1)) > 0){
						write(temp, buf, 1);
					}
					close(out_fd);
					out_fd = open(red_path, O_TRUNC | O_WRONLY); 
				}
				close(temp);
			}
			else {
				ERROR();
				goto end;
			}		
		}

		while((read(fd[0], buf, 1)) > 0){
		//fflush(out_fd);
			if(*buf != '\n') write(out_fd, buf, 1);
			else write(out_fd, " ", 1);
		}
		write(out_fd, "\n", 1);
		if(r_flag == 2){
			temp = open("tmp", O_RDONLY, S_IRUSR | S_IWUSR);
			while((read(temp, buf, 1)) > 0){
				write(out_fd, buf, 1);
			}
			close(temp);
			remove("tmp");
		}
	end:
		close(fd[0]);
		close(out_fd);
		while (waitpid(p, &stat, 0) != p);
	}

}

