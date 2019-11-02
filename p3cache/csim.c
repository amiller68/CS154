#define _GNU_SOURCE
#include <math.h>
#include "cachelab.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>

typedef struct instr{
	char inst;
	int addr;
} instr;

typedef struct Block{
	int val;
	int tag;
	int b;
} Block;

typedef struct lru_ind{
	int ind;
	struct lru_ind *next;
} lru_ind;

typedef struct LRU{
	lru_ind *head;
	lru_ind *tail;
} LRU;

typedef struct Set{
	int E;
	Block *B;
	LRU *L;	
} Set;

typedef struct Cache{
	int S;
	int E;
	int B;
	Set *Se;
} Cache;

void printCache(Cache *C);
Cache *createCache(int s, int e, int b);
void lineToinstr(char *line, instr *i);
void exec(Cache *C, instr *i);

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
	Cache *C = NULL;
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
	
	C = createCache(s, e, b);
	printCache(C);
	
	trace = fopen(t,"r");
	if (trace== NULL){ printf("No Trace\n"); exit(1);}
  	
  	while ((read = getline(&line, &len, trace)) != EOF){
		if(verb) printf("%s", line);
		lineToinstr(line, in);
    }
	
	
	free(in);
	free(line);
	fclose(trace);



    printSummary(mc, hc, ec);
    return 0;
}

Cache *createCache(int s, int e, int b){
	int S = pow(2, s);
	int E = e;
	int B = pow(2, b);
	
	Cache *C = malloc(sizeof(Cache));
	Block *bl = malloc(S*E*sizeof(Block));
	lru_ind *l = malloc(S*E*sizeof(lru_ind));
	Set *Se = malloc(S*sizeof(Set));
	LRU *L = malloc(S*sizeof(LRU));
	
	int ind = -1;
	int i = -1;
	//fill init values of lru_indices + blocks
	for(i = 0; i < S*E; i++){
		bl[i].val = 0;
		bl[i].tag = 0;
		bl[i].b = b;
		ind = i % E;
		l[i].ind = ind;
		if(ind == (E -1)) l[i].next = NULL;
		else l[i].next = &l[i] + 1;		
	} 	

	for(i=0; i<S; i++){
		Se[i].E = E;
		Se[i].B = bl;
		//inc b pointer by E indices
		bl += E;
		L->head = l;
		L->tail = l + (E - 1);
		Se[i].L = L;
		l += E;		
		L++; 
	}

	C->S = S;
	C->E = E;
	C->B = B;
	C->Se = Se;
	
	return C;
}

void printCache(Cache *C){
	printf("S: %d | E: %d | B: %d\n", C->S, C->E, C->B);
	int i = -1;
	int j = -1;
	Set *Se = NULL;
	Block *bl = NULL;
	for(i=0; i < C->S; i++){
		Se = C->Se;
		printf("SET %d:\n", i);
		lru_ind *l = (Se->L)->head;
		while(l != NULL){
			printf("%d->", l->ind);			
			l = l->next;
 		}
		printf("\n");
		
		for(j=0; j < Se->E; j ++){
			bl = Se->B;
			printf("b %d: %d %d %d\n", j, bl[j].val, bl[j].tag, bl[j].b);
		}
	}
}

void lineToinstr(char *line, instr *i){
	char *ch = NULL;
	ch = strsep(&line, " ");
	ch = strsep(&line, " ");
	i->inst = ch[0];
	ch = strsep(&line, ",");
	i->addr = strtol(ch, NULL, 16);
	return;
}

