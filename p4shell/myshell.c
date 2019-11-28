#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>

void ERROR(){
	char error_message[30] = "An error has occurred\n";
	write(STDOUT_FILENO, error_message, strlen(error_message));
}

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
	//512 char plus newline plus null
	char cmd_buff[514];
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
		if(access(argv[1], F_OK)){
			ERROR();
			exit(2);
		}
		batch = fopen(argv[1], "r");
		if(!batch) exit(2);
	}
	
	while (1) {
		
		if(batch){
batch_top:
			if((read = getline(&line, &len, batch)) == -1) break;

			int i = 0;
			while(line[i] != '\n'){
				if((line[i] != ' ') && (line[i] != '	')){
					myPrint(line);
					break;
				}
				i++;	
			}

			strncpy(cmd_buff, line, 514);
			char *cut;
			if((cut = strchr(cmd_buff, '\n'))){
				*cut = '\0';
			}
			else {
				ERROR();
	//			while(fgetc(stdin)!='\n');
				goto batch_top;
			}
		}
		
		else{
shell_top:
			myPrint("myshell> ");
			pinput = fgets(cmd_buff, 514, stdin);
			if (!pinput) {
				ERROR();
				exit(2);
			}
			char *cut;
			if((cut = strchr(cmd_buff, '\n'))){
				*cut = '\0';
			}
			else {
				ERROR();
				while(fgetc(stdin)!='\n');
				goto shell_top;
			}
		}

		pinput = cmd_buff;

		cmd = strtok_r(pinput, ";", &save);
		while(cmd != NULL){

			//puts cmd into terms exec can run
			parse(cmd, args, &r_flag, &red_path);
			//if no redirect, should return NULL
			if(args[0] == NULL){
				if(r_flag) ERROR();
				goto end;
			}
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
					int er;
					if( args[1] ) er = chdir(args[1]);
					else chdir(getenv("HOME"));
					if(er) ERROR();
				}
			}

			else {
				exec(args, r_flag, red_path);
			}
end:
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
		*red_path += 2;
		*red_path = strtok(*red_path, " 	");
		*r_flag = 2;
		if((strtok(NULL, " 	"))){
			args[c] = NULL;
			red_path = NULL;
			goto parse_end;
		}
	}
	
	//find redirection in line
	else if( (*red_path = strstr(cmd, ">")) ) //if redi exists
	{
		*red_path += 1;
		*red_path = strtok(*red_path, " 	");
		*r_flag = 1;
		if((strtok(NULL, "	 "))){
			args[c] = NULL;
			red_path = NULL;
			goto parse_end;
		}
	}

	//Edge case of no space
	if(*cmd == '>'){
		path = NULL;
	}
	
	else {
		cmd = strtok(cmd, ">");
		//if no leading whitespace, gets first arg
		//If nothing but whitespace; returns NULL
		path = strtok(cmd, " 	"); // get first arg of cmd
	}

	args[c] = path;
	//if path is null; first cmd is null
	while(path != NULL){
		args[c++] = path;
		path = strtok(NULL, " 	");	
	}	
parse_end:
	args[c++] = NULL; //add terminating char
}


void exec(char **args, int r_flag, char *red_path)
{
	pid_t p;

	int status;

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
		int err_save = dup(2);
		int err_rt = open("/dev/null", O_WRONLY);
		if(r_flag){
			dup2(err_rt, 2);
		}
	
		close(err_rt);
		close(fd[0]);
		dup2(fd[1], STDOUT_FILENO);
		close(fd[1]);
		if(execvp(args[0], args) < 0){
			ERROR();
			dup2(out_fd, STDOUT_FILENO);
			exit(2);
		}
		//Once done, make sure STDOUT returned to normal
		dup2(out_fd, STDOUT_FILENO);
		dup2(err_save, STDERR_FILENO);
		exit(0);
	}
	
	//Succesful Fork; Parent
	else{
		close(fd[1]);
		char buf[1];
		int temp;
		if(r_flag){
			struct stat s;
			int no_dir;
			no_dir = stat(dirname(red_path), &s);
			if(no_dir){
				ERROR();
				goto end;
			}
		}
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
			write(out_fd, buf, 1);
		}
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
		while (waitpid(p, &status, 0) != p);
	}

}

