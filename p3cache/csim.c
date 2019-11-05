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
	int s;
	int E;
	int b;
	Set *S;
} Cache;

void printCache(Cache *C);
Cache *createCache(int s, int e, int b);
void lineToinstr(char *line, instr *i);
void exec(Cache *C, instr *i, int *hc, int *mc, int *ec, int v);

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
	//printCache(C);
	
	trace = fopen(t,"r");
	if (trace== NULL){ printf("No Trace\n"); exit(1);}
  	
  	while ((read = getline(&line, &len, trace)) != EOF){
		if(verb) printf("%s", strtok(line, "\n"));
		lineToinstr(line, in);
		exec(C, in, &hc, &mc, &ec, verb);
    }
	
	
	free(in);
	free(line);
	fclose(trace);



    printSummary(mc, hc, ec);
    return 0;
}


void update_lru(LRU *L, int ind){
	lru_ind *token = L->head;
	//Head of list is MRU
	//Tail of list is LRU
	if (token->ind == ind) return;
	lru_ind *next = NULL;
	while(token->next != NULL){
		next = token->next;
		if(next->ind == ind){
			token->next = next->next;
			next->next = L->head;
			L->head = next;
		}
		token = next;
	}

	return;
}

void exec(Cache *C, instr *i, int *hc, int *mc, int *ec, int v){
	int proc = i->inst;
	switch(proc){
		case 'S':
			proc = 0;
		case 'L':
			proc = 0;
		case 'M':
			proc = 1;
		default:
			return;
	}

	int b = C->b;
	int s = C->s;

	i->addr >>= b;
	int s_mask = (1 << s) - 1;
	int s_bits = i->addr & s_mask;
	i->addr >>= s;
	int t_bits = i->addr;
	
	Set *S = &(C->S)[s_bits];
	Block *Set = S->B;
	LRU *L = S->L;
	int j = 0;
	int found = 0; 

M_instr:

	for( ; j < C->E; j++){
		//Compulsory Miss
		if(!(Set[j].val)){
			(*mc)++;
			if(v) printf(" miss " );
			Set[j].val = 1;
			break;
		}
		//hit
		if(Set[j].tag == t_bits){
			(*hc)++;
			Set[j].val = 1;
			if(v) printf(" hit ");
			found = 1;
			break;
		}
	}
	

	//Capacity and Conflict
	if(!found){
		j = (L->tail)->ind;
		Set[j].tag = t_bits;
		if(proc){
			if(v) printf(" eviction ");
			(*ec)++;
			goto M_instr;
		}
	}
	
	update_lru(L , j);

	if(v) printf("\n");

	return;

}

Cache *createCache(int s, int e, int b){
	int S = pow(2, s);
	int E = e;
	
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
		ind = (E-i) % E;
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

	C->s = s;
	C->E = E;
	C->b = b;
	C->S = Se;
	
	return C;
}

void printCache(Cache *C){
	printf("s: %d | E: %d | b: %d\n", C->s, C->E, C->b);
	int i = -1;
	int j = -1;
	Set *Se = NULL;
	Block *bl = NULL;
	int S = pow(2, C->s);
	for(i=0; i < S; i++){
		Se = C->S;
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

