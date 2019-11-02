#define _GNU_SOURCE
#include "cachelab.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>

typedef struct inst{
	char inst;
	int addr;
	int val;
} instr;

typedef struct cac{
	int S;
	int E;
	int B;
} cache;

void lineToinstr(char *line, instr *i);
void exec(cache *C, instr *i);

int main(int argc, char **argv)
{
	int res;
	int s, e, b, hc, mc, ec;
	s = -1; e = -1; b = -1; hc = 0; mc = 0; ec = 0;
	char *t = "";
	int verb = 0;
	FILE *trace = NULL;
	char *line = NULL;
	size_t len = 0;
	ssize_t read = 0;	
	instr *in = malloc(sizeof(instr));

	while((res = getopt(argc, argv, "vs:e:b:t:")) != -1){
		switch(res){
			case 'v':
				verb = 1;
				break;
			case 's':
				s = atoi(optarg);
				break;
			case 'e':
				e = atoi(optarg);
				break;
			case 'b':
				b = atoi(optarg);
				break;
			case 't':
				t = optarg;
				break;
			default:
				printf("Input Error\n");
				exit(1);
		}
	}

	trace = fopen(t,"r");
	if (trace== NULL){ printf("No Trace\n"); exit(1);}
  	
  	while ((read = getline(&line, &len, trace)) != EOF){
		if(verb) printf("%s", line);
		lineToinstr(line, in);
    }
	
	free(in);
	free(line);
	fclose(trace);

	printf("-v=%d\n-s=%d\n-e=%d\n-b=%d\n-t=%s\n", verb, s, e, b, t);

	
	
	

//allocate size of cache
//-> grid? or array? multi array for associativity
//hit count // miss count
//create -v option
    printSummary(mc, hc, ec);
    return 0;
}

void lineToinstr(char *line, instr *i){
	char *ch = NULL;
	ch = strsep(&line, " ");
	ch = strsep(&line, " ");
	i->inst = ch[0];
	ch = strsep(&line, ",");
	i->addr = atoi(ch);
	ch = strsep(&line, "");
	i->val = atoi(ch);	

	return;
}

