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
} Block;

typedef struct LRU_header LRU;
typedef struct lru_ind_header lru_ind;

struct lru_ind_header{
	int ind;
	struct lru_ind_header *next;
};

struct LRU_header{
	lru_ind *head;
	lru_ind *tail;
};

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

	//for free
	Block *bl;
	lru_ind *l;
	LRU *L;
} Cache;

void printCache(Cache *C);
Cache *createCache(int s, int e, int b);
void lineToinstr(char *line, instr *i);
void exec(Cache *C, instr *i, int *hc, int *mc, int *ec, int v);
void free_cache(Cache *C);

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
	if(in == NULL){ exit(1); }
	Cache *C = NULL;
	while((res = getopt(argc, argv, "vs:E:b:t:")) != -1){
		switch(res){
			case 'v':
				verb = 1;
				break;
			case 's':
				s = atoi(optarg);
				break;
			case 'E':
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
	trace = fopen(t,"r");
	if (trace== NULL){ printf("No Trace\n"); exit(1);}
  	while ((read = getline(&line, &len, trace)) != EOF){
		line = strtok(line, "\n");
		
		if(verb) printf("%s", line);
		
		lineToinstr(line, in);


		exec(C, in, &hc, &mc, &ec, verb);
    }
	
	
    printSummary(hc, mc, ec);
	free(in);
	free(line);
	free_cache(C);
	fclose(trace);

    return 0;
}

void print_L(LRU *L){
	lru_ind *l = L->head;
	printf("%d -> ", l->ind);
	while (l->next != NULL){
	
		l = l->next;
		printf("%d -> ", l->ind);
	}
	printf("\n");
	return;
}

void update_lru(LRU *L, int i){
	lru_ind *token = L->head;
	//Head of list is MRU
	//Tail of list is LRU
	if (token->ind == i){
		return;
	}

	lru_ind *next = NULL;
	while(token->next != NULL){
		next = token->next;
		if((next->ind) == i){
			token->next = next->next;
			next->next = L->head;
			L->head = next;
		}
		token = next;
	}
	L->tail = token;
	return;
}

void exec(Cache *C, instr *i, int *hc, int *mc, int *ec, int v){
	int proc = -1;
	switch(i->inst){
		case 'S':
			proc = 0;
			break;
		case 'L':
			proc = 0;
			break;
		case 'M':
			proc = 1;
			break;
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
	
	Set *S = (C->S) + s_bits;
	Block *Set = S->B;
	LRU *L = S->L;
	int j = 0;
	int found = 0; 

	for( ; j < C->E; j++){
		//Compulsory Miss
		if(!(Set[j].val)){
			(*mc)++;
			if(v) printf(" miss " );
			Set[j].val = 1;
			Set[j].tag = t_bits;
			found = 1;
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
		if(v) printf(" miss eviction ");
		(*ec)++;
		(*mc)++;
		found = 1;
	}

	if(proc){
		if(v) printf(" hit ");
		(*hc)++;
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

	if(C == NULL || Se == NULL || bl == NULL || l == NULL || L == NULL){
		exit(1);
	}	

	//for Freeing purposes
	C->bl = bl;
	C->l = l;
	C->L = L;
	C->S = Se;
	C->E = E;
	C->s = s;
	C->b = b;

	int ind = -1;
	int i = -1;
	//fill init values of lru_indices + blocks
	for(i = 0; i < S*E; i++){
		bl->val = 0;
		bl->tag = -1;
		ind = i % E;
		l->ind = ind;
		if(ind == (E -1)) l->next = NULL;
		else l->next = l + 1;
		l++; bl++;		
	} 	

	l = C->l; bl = C->bl;

	for(i=0; i<S; i++){
		Se->E = E;
		Se->B = bl;
		//inc b pointer by E indices
		bl += E;
		L->head = l;
		L->tail = l + (E - 1);
		Se->L = L;
		l += E;		
		L++;
		Se++; 
	}


	
	return C;
}

void free_cache(Cache *C){
	Block *bl = C->bl;
	lru_ind *l = C->l;
	Set *Se = C->S;
	LRU *L = C->L; 
		free(bl);
		free(l);
		free(Se);
		free(L);

	free(C);	
	return;
}
/*
void printCache(Cache *C){
	printf("s: %d | E: %d | b: %d\n", C->s, C->E, C->b);
	int i = -1;
	int j = -1;
	Set *Se = C->S;
	Block *bl = NULL;
	lru_ind *l = NULL;
	int S = pow(2, C->s);
	LRU *L = NULL;
	for(i=0; i < S; i++){
		printf("SET %d:\n", i);
		L = Se->L;
		l = L->head;
		while(l != NULL){
			printf("%d->", l->ind);			
			l = l->next;
 		}
		printf("\n");
		
		bl = Se->B;
		for(j=0; j < Se->E; j ++){
			printf("b %d: %d %d\n", j, bl->val, bl->tag);
			bl++;
		}
		Se++;
	}
}
*/
void lineToinstr(char *line, instr *i){
	char *ch = NULL;
	ch = strsep(&line, " ");
	ch = strsep(&line, " ");
	i->inst = ch[0];
	ch = strsep(&line, ",");
	i->addr = strtol(ch, NULL, 16);
	return;
}

