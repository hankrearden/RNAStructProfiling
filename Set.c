#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Set.h"
#include "hashtbl.h"
#include "Options.h"
#include "helix_class.h"
#include "Profile.h"
//#include "Node.h"

static HASHTBL *bp;
static HASHTBL *translate_hc;
static HASHTBL *consensus;

Set* make_Set(char *name) {
  Set *set = malloc(sizeof(Set));
  set->seq = NULL;
  set->structfile = name;
  set->hc_size = 5;
  set->hc_num = 0;
  set->num_fhc = 0;
  set->helices = malloc(sizeof(HC*)*ARRAYSIZE*5);
  set->prof_size = 5;
  set->prof_num = 0;
  set->num_sprof = 0;
  set->profiles = malloc(sizeof(Profile*)*ARRAYSIZE*5);
  set->h_cutoff = 0;
  set->p_cutoff = 0;
  set->opt = make_options();
  set->graph = NULL;
  return set;
}

void input_seq(Set *set,char *seqfile) {
  FILE * fp;
  int size = 5,fasta = 0;
  char temp[100],*blank = " \n",*part,*final;

  fp = fopen(seqfile,"r");
  if (fp == NULL) {
    fprintf(stderr, "can't open %s\n",seqfile);
  }
  final = malloc(sizeof(char)*ARRAYSIZE*size);
  final[0] = '\0';
  while (fgets(temp,100,fp)) {    
    //put error handling in case first line has more than 100 chars
    //skipping first line if fasta header and not seq
    if (temp[0] == '>' || fasta) {
      //printf("found fasta header %s",temp);
      if (strlen(temp) < 99 || (strlen(temp) == 99 && temp[98] == '\n')) {
	fasta = 0;
	continue;
      }
      else 
	fasta = 1;
      continue;
    } 
    for (part = strtok(temp,blank); part; part = strtok(NULL,blank)) {
      if (strlen(final)+strlen(part) > ARRAYSIZE*size-1) {
	while (++size*ARRAYSIZE - 1 < strlen(final)+strlen(part)) ;
	final = realloc(final,sizeof(char)*ARRAYSIZE*size);
      }
      final = strcat(final,part);
    }
  }
  if (set->opt->VERBOSE) 
    printf("seq in %s is %s with length %d\n",seqfile,final,strlen(final));
  //printf("final char is %c\n",final[strlen(final)-1]);
  set->seq = final;
}

void process_structs(Set *set) {
  FILE *fp;
  int i,j,k,*helixid,idcount=1,*lg,last = 0, toosmall = 0;
  double *trip;
  char tmp[100],*key,dbl[ARRAYSIZE],*max;
  HASHTBL *extra,*avetrip;
  HC *hc;

  fp = fopen(set->structfile,"r");
  if (fp == NULL) {
    fprintf(stderr, "can't open %s\n",set->structfile);
  }
  if (!(bp = hashtbl_create(HASHSIZE,NULL))) {
    fprintf(stderr, "ERROR: hashtbl_create() for bp failed");
    exit(EXIT_FAILURE);
  }
  if (!(avetrip = hashtbl_create(HASHSIZE,NULL))) {
    fprintf(stderr, "ERROR: hashtbl_create() for avetrip failed");
    exit(EXIT_FAILURE);
  }
  if (!(extra = hashtbl_create(HASHSIZE,NULL))) {
    fprintf(stderr, "ERROR: hashtbl_create() for extra failed");
    exit(EXIT_FAILURE);
  }
  key = malloc(sizeof(char)*ARRAYSIZE);
  while (fgets(tmp,100,fp) != NULL) {
    if (sscanf(tmp,"%d %d %d",&i,&j,&k) == 3) {
      if (i == 0) continue;
      if (k < set->opt->MIN_HEL_LEN) {
	toosmall++;
	//printf("too small %d %d %d\n",i,j,k);
	continue;
      }
      sprintf(dbl,"%d %d",i,j);
      helixid = hashtbl_get(bp,dbl);
      //printf("%d %d %d\n",i,j,k);
      if (!helixid) {
	max = longest_possible(idcount,i,j,k,set->seq);
	hc = create_HC(idcount,max);
	addHC(set,hc,idcount);
	//triplet stats
	trip = malloc(sizeof(double)*3);
	trip[0] = i;
	trip[1] = j;
	trip[2] = k;
	sprintf(key,"%d",idcount);
	hashtbl_insert(avetrip,key,trip);

	last = idcount++;
      }
      else {
	//printf("Found %d %d with id %d\n",i,j,*helixid);
	sprintf(key,"%d",*helixid);
	if (last != *helixid) {
	  hc = set->helices[*helixid-1];
	  hc->freq++;
	} else {
	  if ((lg = hashtbl_get(extra,key)))
	    ++*lg;
	  else {
	    lg = malloc(sizeof(int));
	    *lg = 1;
	    hashtbl_insert(extra,key,lg);
	  }
	  //if (VERBOSE) 
	  //printf("Found repeat id %d:%s\n",last,hashtbl_get(idhash,key));
	}
	//average stats
	trip = hashtbl_get(avetrip,key);
	trip[0] += i;
	trip[1] += j;
	trip[2] += k;
	
	last = *helixid;
      }
    }
    else if (sscanf(tmp,"Structure %d",&i) == 1)
      set->opt->NUMSTRUCTS++;
  }
  for (i = 1; i < idcount; i++) {
    j = set->helices[i-1]->freq;
    sprintf(key,"%d",i);
    if ((lg = hashtbl_get(extra,key))) 
      k = j + *lg;
    else
      k = j;
    trip = hashtbl_get(avetrip,key);
    sprintf(key,"%.1f %.1f %.1f", trip[0]/k,trip[1]/k,trip[2]/k);
    hc = set->helices[i-1];
    hc->avetrip = key;
  }
  if (fclose(fp))
    fprintf(stderr, "File %s not closed successfully\n",set->structfile);
  hashtbl_destroy(extra);
  hashtbl_destroy(avetrip);
}

char* longest_possible(int id,int i,int j,int k,char *seq) {
  int m = 1,*check,*num,diff;
  char *val;

  val = malloc(sizeof(char)*ARRAYSIZE);
  for (diff = j-i-2*(k+1); diff >= 2 && match(i+k,j-k,seq); diff = j-i-2*(k+1)) k++;
  //if (diff < 2 && match(i+k,j-k)) printf("found overlap for %d %d %d\n",i,j,k+1);
  while (match(i-m,j+m,seq)) m++;
  m--;
  i -= m;
  j += m;
  k+= m;
  sprintf(val,"%d %d %d",i,j,k);

  num = malloc(sizeof(int));
  *num = id;
  for (m = 0; m < k; m++) {
    sprintf(val,"%d %d",i+m,j-m);
    if ((check = hashtbl_get(bp,val)))
      printf("%s (id %d) already has id %d\n",val,id,*check);
    hashtbl_insert(bp,val,num);
  }
  sprintf(val,"%d %d %d",i,j,k);
  return val;
}

//finds longest possible helix based on i,j
//populates all bp in longest possible with id in hash
int match(int i,int j,char *seq) {
  char l,r;
  if (i >= j) return 0;
  if (i < 1) return 0;
  if (j > strlen(seq)) return 0;
  l = seq[i-1];
  r = seq[j-1];
  //printf("l(%d) is %c and r(%d) is %c\n",i,l,j,r);
  if ((l == 'a' || l == 'A') && (r == 'u' || r == 'U' || r == 't' || r == 'T'))
    return 1;
  else if ((l == 'u' || l == 'U' || l == 't' || l == 'T') && (r == 'a' || r == 'A' || r == 'g' || r == 'G'))
    return 1;
  else if ((l == 'c' || l == 'C') && (r == 'g' || r == 'G'))
    return 1;
  else if ((l == 'g' || l == 'G') && (r == 'c' || r == 'C' || r == 'u' || r == 'U' || r == 't' || r == 'T' ))
    return 1;
  else
    return 0;
}

void addHC(Set *set, HC *hc, int idcount) {
  if (idcount > ARRAYSIZE*set->hc_size) {
    set->hc_size++;
    set->helices = realloc(set->helices, sizeof(HC*)*ARRAYSIZE*set->hc_size);
  } 
  set->helices[idcount-1] = hc;
  set->hc_num = idcount;
}

void reorder_helices(Set *set) {
  int i,*new,total;
  char *old;
  HC **helices = set->helices;

  total = set->hc_num;
  if (!(translate_hc = hashtbl_create(HASHSIZE,NULL))) {
    fprintf(stderr, "ERROR: hashtbl_create() for translate_hc failed");
    exit(EXIT_FAILURE);
  }

  qsort(helices,total,sizeof(HC*),freqcompare);
  for (i = 0; i < total; i++) {
    old = helices[i]->id;
    new = malloc(sizeof(int)*ARRAYSIZE);
    *new = i+1;
    hashtbl_insert(translate_hc,old,new);
    sprintf(old,"%d",i+1);
  }
}

//will sort to have descending freq
int freqcompare(const void *v1, const void *v2) {
  HC *h1,*h2;
  h1 = *((HC**)v1);
  int i1 = h1->freq;
  h2 = *((HC**)v2);
  int i2 = h2->freq;
  return (i2 - i1);
}

double set_threshold(Set *set, int start) {
  int *helices,i,sum=0,freq_target,ave = 0,*dropoffs,diff,*val,partial=0,total;
  char key[ARRAYSIZE];
  double h, frac;
  //where you start in drop off calc has to cover at least 50% of area
  double coverage = 50.0;
  HASHTBL *diff_to_key;
  HC **list = set->helices;

  total = set->hc_num;

  if (!(diff_to_key = hashtbl_create(HASHSIZE,NULL))) {
    fprintf(stderr, "ERROR: hashtbl_create() for diff_to_key failed");
    exit(EXIT_FAILURE);
  }

  //translate start percentage to actual num of elements
  freq_target = start*set->opt->NUMSTRUCTS/100;

  helices = malloc(sizeof(int)*total);
  //initialize top three dropoff locations
  dropoffs = malloc(sizeof(int)*3);
  for (i = 0; i<3; dropoffs[i++] = 0) ;
  i = 0;
  for (i = 0; i < total; i++) {
    helices[i] = list[i]->freq;
    //printf("helices[%d] = %d\n",i,helices[i]);
    sum += helices[i];
  }

  qsort(helices,total,sizeof(int),compare);
  //printf("freq target is %d with sum %d last num %d at %d\n",freq_target,sum,helices[i-1],i);
  //for (i = 0; i < total; i++)
  //printf("helices[%d] is %d\n",i,helices[i]);
  for (--i; helices[i] > freq_target && i >= 0; i--)
    partial += helices[i];
  //if everything above start, return the start
  if (i < 0)
    return (double) start;

  //if stopping at freq_target results in less than 50% coverage, go to at least 50%
  frac = ((double) partial*100)/((double)sum);
  if (frac < coverage) {
    //printf("not up to coverage with %d/%d\n",partial,sum);
    while (frac < coverage) {
      partial += helices[i--];
      frac = ((double) partial*100)/((double)sum);
    }
    //printf("now at %d/%d = %.1f at helices[%d] = %d\n",partial,sum,frac,i+1,helices[i+1]);
  }
  //create entry for 0, in case no drop off exists
  sprintf(key,"%d",0);
  val = malloc(sizeof(int));
  *val = helices[i];
  hashtbl_insert(diff_to_key,key,val);

  for ( ; i > 0; i--) {
    ave = (helices[i+1]+helices[i-1])/2;
    diff = ave - helices[i];
    //printf("ave is %d and diff is %d for %d\n",ave,diff,i);
    if (diff > dropoffs[0]) {
      //printf("bumping off %d for %d\n",dropoffs[0],diff);
      dropoffs[0] = diff;
      qsort(dropoffs,3,sizeof(int),compare);
      sprintf(key,"%d",diff);
      val = malloc(sizeof(int));
      *val = helices[i];
      hashtbl_insert(diff_to_key,key,val);
      //printf("inserting %s with %d\n",key,*val);
    }
  }
  printf("Possible cutoffs: ");
  for (i = 0; i<3; i++) {
    sprintf(key,"%d",dropoffs[i]);
    val = hashtbl_get(diff_to_key,key);
    if (val) {
      h = ((double)(*val+1)*100)/(double)set->opt->NUMSTRUCTS;
      printf("%.1f ",h);
    }
  }
  printf("\n");
  hashtbl_destroy(diff_to_key);
  return h;
}

int compare(const void *v1, const void *v2) {
  return (*(int*)v1 - *(int*)v2);
}

//looks up and prints actual helices for all id's
void print_all_helices(Set *set) {
  HC **helices = set->helices;
  char *val,*trip;
  int i,m,total = set->hc_num;

  for (i = 0; i < total; i++) {
    val = helices[i]->maxtrip;
    m = helices[i]->freq;
    trip = helices[i]->avetrip;
    if (val != NULL)
      printf("Helix %d is %s (%s) with freq %d\n",i+1,val,trip,m);
    else
      printf("No entry for %d\n",i);
   }
 }

void find_freq(Set *set) {
  int marg,i,total;
  double percent;
  HC *hc;

  total = set->hc_num;
  for (i = 0; i < total; i++) {
    hc = set->helices[i];
    marg = hc->freq;
    percent = ((double) marg)*100.0/((double)set->opt->NUMSTRUCTS);
    if (percent >= set->opt->HC_FREQ) {
      if (set->opt->VERBOSE)
	printf("freq helix %d: %s with freq %d\n",i+1,hc->maxtrip,marg);
      hc->isfreq = 1;      
      hc->binary = 1<<i;
    }
    else {
      set->num_fhc = i;
      i = total;
    }
  } 
    if (set->num_fhc > 63) fprintf(stderr,"number of helices greater than allowed in find_freq()\n");
    
}

void make_profiles(Set *set) {
  FILE *fp,*file;
  int num=0,*id = 0,i,j,k,last = -1, lastfreq = -1,*profile;
  int numhelix = 0,size=1,tripsize = INIT_SIZE;
  char temp[100],val[ARRAYSIZE],*trips,*name,*prof;
  HASHTBL *halfbrac;

  name = set->structfile;
  profile = malloc(sizeof(int)*ARRAYSIZE);

  if (!(halfbrac = hashtbl_create(HASHSIZE,NULL))) {
    fprintf(stderr, "ERROR: hashtbl_create() for halfbrac failed");
    exit(EXIT_FAILURE);
  }
  if (set->opt->REP_STRUCT) {
    if (!(consensus = hashtbl_create(HASHSIZE,NULL))) {
      fprintf(stderr, "ERROR: hashtbl_create() for consensus failed");
      exit(EXIT_FAILURE);
    }
    trips = malloc(sizeof(char)*tripsize*ARRAYSIZE);
    trips[0] = '\0';
  }
  fp = fopen(name,"r");
  file = fopen("structure.out","w");
  fprintf(file,"Processing %s\n",name);
  if (fp == NULL) {
    fprintf(stderr, "can't open %s\n",name);
    return;
  }
  while (fgets(temp,100,fp) != NULL) {
    if (sscanf(temp,"Structure %d",&num) == 1) {
     if (last == -1) {
       fprintf(file,"Structure %d: ",num);	
       continue;
     }
     prof = process_profile(halfbrac,profile,numhelix,set);
     fprintf(file,"\n\t-> %s\nStructure %d: ",prof,num);
     
     if (set->opt->REP_STRUCT) {
       make_rep_struct(consensus,prof,trips);
     }
     if (!(halfbrac = hashtbl_create(HASHSIZE,NULL))) {
       fprintf(stderr, "ERROR: hashtbl_create() for halfbrac failed");
       exit(EXIT_FAILURE);
     }
     last = 0;
     numhelix = 0;
     if (set->opt->REP_STRUCT)
       trips[0] = '\0';
    } else if (sscanf(temp,"%d %d %d",&i,&j,&k) == 3) {
      sprintf(val,"%d %d",i,j);
      id = hashtbl_get(bp,val);
      sprintf(val,"%d",*id);
      id = hashtbl_get(translate_hc,val);
      if (!id) continue;
      if (*id != -1 && *id != last) {
	fprintf(file,"%d ",*id);
	if (*id <= set->num_fhc && *id != lastfreq) {
	  numhelix++;
	  if (numhelix >= ARRAYSIZE*size) 
	    profile = realloc(profile,ARRAYSIZE*++size);
	  profile[numhelix-1] = *id;
	  make_brackets(halfbrac,i,j,*id);
	  lastfreq = *id;
	}
	last = *id;
      }
      if (set->opt->REP_STRUCT) {
	sprintf(val,"%d %d %d ",i,j,k);
	while (strlen(trips)+strlen(val) > (ARRAYSIZE*tripsize-1))
	  trips = realloc(trips,++tripsize*ARRAYSIZE);
	strcat(trips,val);
      }
    }
  }
  fprintf(file,"\n\t-> %s ",prof);
 
  process_profile(halfbrac,profile,numhelix,set);
 //fprintf(file,"Structure %d: %s\n",num,profile);
 
  free(profile);
  fclose(fp);
  fclose(file);
}

char* process_profile(HASHTBL *halfbrac,int *profile,int numhelix,Set *set) {
  int i,size=INIT_SIZE;
  char val[ARRAYSIZE],*dup;
  Profile **profiles;  

  profiles = set->profiles;
  qsort(profile,numhelix,sizeof(int),compare);

  dup = malloc(sizeof(char)*ARRAYSIZE*size);
  dup[0] = '\0';
  for (i = 0; i < numhelix; i++) {
    sprintf(val,"%d ",profile[i]);
    if (strlen(dup) + strlen(val) >= ARRAYSIZE*size-1)
      dup = realloc(dup,ARRAYSIZE*++size);
    dup = strcat(dup,val);
  }

  for (i = 0; i < set->prof_num; i++) {
    if (!strcmp(profiles[i]->profile,dup)) {
      profiles[i]->freq++;
      break;
    }
  }
  if (i == set->prof_num) {
    if (i >= ARRAYSIZE*set->prof_size) {
      set->prof_size++;
      profiles = realloc(profiles,ARRAYSIZE*set->prof_size);
      set->profiles = profiles;
    }
    profiles[i] = create_profile(dup);
    //printf("adding profile[%d] %s\n",i,profiles[i]->profile);
    set->prof_num++;
    make_bracket_rep(halfbrac,profiles[i]);
  }
  hashtbl_destroy(halfbrac);
  return dup;
}

//makes the bracket representation of dup, using values in hashtbl brac
//dup is a (mod) profile in graph
//called by process_profile()
void make_bracket_rep(HASHTBL *brac,Profile *prof) {
  int num,*array,k=0,size = INIT_SIZE,total;
  char *profile,*val;
  KEY *node = NULL;

  num = hashtbl_numkeys(brac);
  array = malloc(sizeof(int)*num);
  for (node = hashtbl_getkeys(brac); node; node=node->next) 
    array[k++] = atoi(node->data);
  //sort by i,j position  
  qsort(array,num,sizeof(int),compare);
  profile = malloc(sizeof(char)*ARRAYSIZE*size);
  profile[0] = '\0';
  val = malloc(sizeof(char)*ARRAYSIZE);
  for (k = 0; k < num; k++) {
    sprintf(val,"%d",array[k]);
    val = hashtbl_get(brac,val);
    if ((total = strlen(profile)+strlen(val)) > ARRAYSIZE*size-1)
      profile = realloc(profile,sizeof(char)*ARRAYSIZE*++size);
    strcat(profile,val);
  }
  prof->bracket = profile;
  //free(val);
  free(array);
}

//inserts bracket representation for i,j into a hash
void make_brackets(HASHTBL *brac, int i, int j, int id) {
  char key[ARRAYSIZE],*val;

  sprintf(key,"%d",i);
  val = malloc(sizeof(char)*ARRAYSIZE);
  sprintf(val,"[%d",id);
  //  printf("making bracket %s for %d\n",val,i);
  hashtbl_insert(brac,key,val);
  sprintf(key,"%d",j);
  val = malloc(sizeof(char)*2);
  val[0] = ']';
  val[1] = '\0';
  hashtbl_insert(brac,key,val);
}

void make_rep_struct(HASHTBL *consensus,char *profile, char* trips) {
  int *bpfreq,i,j,k;
  char *val,*blank = " ",bpair[ARRAYSIZE];
  HASHTBL *ij;
  
  ij = hashtbl_get(consensus,profile);
  if (!ij) {
    if (!(ij = hashtbl_create(HASHSIZE,NULL))) {
      fprintf(stderr, "ERROR: hashtbl_create() for ij failed");
      exit(EXIT_FAILURE);
    }
    hashtbl_insert(consensus,profile,ij);
  }
  for (val = strtok(trips,blank); val; val = strtok(NULL,blank)) {
    i = atoi(val);
    j = atoi(strtok(NULL,blank));
    k = atoi(strtok(NULL,blank));
    for (k--; k >= 0; k--) {
      sprintf(bpair,"%d %d",i+k,j-k);
      bpfreq = hashtbl_get(ij,bpair);
      if (bpfreq)
	(*bpfreq)++;
      else {
	bpfreq = malloc(sizeof(int));
	*bpfreq = 1;
	hashtbl_insert(ij,bpair,bpfreq);
      }
      //printf("in rep struct for %s, inserting %d %d\n",profile,i+k,j-k);      
    }
  }
}

void print_profiles(Set *set) {
  int i;
  Profile **profiles = set->profiles;
  
  qsort(profiles,set->prof_num,sizeof(Profile*),profsort); 
  for (i = 0; i < set->prof_num; i++) 
    if (set->opt->VERBOSE)
      printf("Profile %s with freq %d\n",profiles[i]->profile,profiles[i]->freq);
}

int profsort(const void *v1, const void *v2) {
  Profile *p1,*p2;
  p1 = *((Profile**)v1);
  p2 = *((Profile**)v2);
  return (p2->freq - p1->freq);
}

double set_p_threshold(Set *set, int start) {
  int *helices,i,sum=0,freq_target,ave = 0,*dropoffs,diff,*val,partial=0,total;
  char key[ARRAYSIZE];
  double h, frac;
  //where you start in drop off calc has to cover at least 50% of area
  double coverage = 50.0;
  HASHTBL *diff_to_key;
  Profile **list = set->profiles;

  total = set->prof_num;

  if (!(diff_to_key = hashtbl_create(HASHSIZE,NULL))) {
    fprintf(stderr, "ERROR: hashtbl_create() for diff_to_key failed");
    exit(EXIT_FAILURE);
  }

  //translate start percentage to actual num of elements
  freq_target = start*set->opt->NUMSTRUCTS/100;

  helices = malloc(sizeof(int)*total);
  //initialize top three dropoff locations
  dropoffs = malloc(sizeof(int)*3);
  for (i = 0; i<3; dropoffs[i++] = 0) ;
  i = 0;
  for (i = 0; i < total; i++) {
    helices[i] = list[i]->freq;
    //printf("helices[%d] = %d\n",i,helices[i]);
    sum += helices[i];
  }

  qsort(helices,total,sizeof(int),compare);
  //printf("freq target is %d with sum %d last num %d at %d\n",freq_target,sum,helices[i-1],i);
  //for (i = 0; i < total; i++)
  //printf("helices[%d] is %d\n",i,helices[i]);
  for (--i; helices[i] > freq_target && i >= 0; i--)
    partial += helices[i];
  //if everything above start, return the start
  if (i < 0)
    return (double) start;

  //if stopping at freq_target results in less than 50% coverage, go to at least 50%
  frac = ((double) partial*100)/((double)sum);
  if (frac < coverage) {
    //printf("not up to coverage with %d/%d\n",partial,sum);
    while (frac < coverage) {
      partial += helices[i--];
      frac = ((double) partial*100)/((double)sum);
    }
    //printf("now at %d/%d = %.1f at helices[%d] = %d\n",partial,sum,frac,i+1,helices[i+1]);
  }
  //create entry for 0, in case no drop off exists
  sprintf(key,"%d",0);
  val = malloc(sizeof(int));
  *val = helices[i];
  hashtbl_insert(diff_to_key,key,val);

  for ( ; i > 0; i--) {
    ave = (helices[i+1]+helices[i-1])/2;
    diff = ave - helices[i];
    //printf("ave is %d and diff is %d for %d\n",ave,diff,i);
    if (diff > dropoffs[0]) {
      //printf("bumping off %d for %d\n",dropoffs[0],diff);
      dropoffs[0] = diff;
      qsort(dropoffs,3,sizeof(int),compare);
      sprintf(key,"%d",diff);
      val = malloc(sizeof(int));
      *val = helices[i];
      hashtbl_insert(diff_to_key,key,val);
      //printf("inserting %s with %d\n",key,*val);
    }
  }
  printf("Possible cutoffs: ");
  for (i = 0; i<3; i++) {
    sprintf(key,"%d",dropoffs[i]);
    val = hashtbl_get(diff_to_key,key);
    if (val) {
      h = ((double)(*val+1)*100)/(double)set->opt->NUMSTRUCTS;
      printf("%.1f ",h);
    }
  }
  printf("\n");
  hashtbl_destroy(diff_to_key);
  return h;
}

void select_profiles(Set *set) {
  int i,coverage=0;
  double percent;
  Profile *prof;

  for (i = 0; i < set->prof_num; i++) {
    prof = set->profiles[i];
    percent = ((double) prof->freq)*100.0 / ((double)set->opt->NUMSTRUCTS);
    //printf("%s has percent %.1f\n",node->data,percent);
    if (percent < set->opt->PROF_FREQ) {
      set->num_sprof = i;
      i = set->prof_num;
    }
    else {
      prof->selected = 1;
      coverage += prof->freq;
      if (set->opt->VERBOSE)
	printf("Freq profile %swith freq %d\n",prof->profile,prof->freq);
    }
  }
  printf("Number of structures with direct representation in graph: %d/%d\n",coverage,set->opt->NUMSTRUCTS);
}

void find_consensus(Set *set) {
  int freq,*bpfreq,i;
  KEY *node,*bpnode;
  HASHTBL *ij,*final;

  if (!(final = hashtbl_create(HASHSIZE,NULL))) {
    fprintf(stderr, "ERROR: hashtbl_create() for final failed");
    exit(EXIT_FAILURE);
  }

  node = hashtbl_getkeys(consensus);
  //cycle thru all profiles, either calc consensus for selected or destroying
  for (node = node->next; node; node = node->next) {
    freq = 0;
    for (i = 0; i < set->num_sprof; i++) 
      if (!strcmp(set->profiles[i]->profile,node->data))
	freq = set->profiles[i]->freq;
    ij = hashtbl_get(consensus,node->data);
    if (freq) {
      if (!ij)
	fprintf(stderr, "ij not found in find_consensus()\n");
      for (bpnode = hashtbl_getkeys(ij); bpnode; bpnode = bpnode->next) {
	bpfreq = hashtbl_get(ij,bpnode->data);
	if (*bpfreq*100/freq < 50)
	  hashtbl_remove(ij,bpnode->data);
      }
    } else {
      hashtbl_destroy(ij);
      //insert dummy pointer so remove won't seg fault
      hashtbl_insert(consensus,node->data, malloc(sizeof(char)));
      hashtbl_remove(consensus,node->data);
    }
  }
}

//in ct format
int print_consensus(Set *set) {
  int k=0,m,seqlen;
  char outfile[ARRAYSIZE],key[ARRAYSIZE],*pair,*i,*j,*blank = " ";
  KEY *node,*bpnode;
  HASHTBL *bpairs,*temp;
  FILE *fp;

  seqlen = strlen(set->seq);

  //foreach profile
  for (node = hashtbl_getkeys(consensus); node; node = node->next) {
    if (!(temp = hashtbl_create(HASHSIZE,NULL))) {
      fprintf(stderr, "ERROR: hashtbl_create() for temp failed");
      exit(EXIT_FAILURE);
    }
    sprintf(outfile,"Structure_%d.ct",++k);
    fp = fopen(outfile,"w");

    //fprintf(fp,"Filename: %s\n",set->seqfile);
    fprintf(fp,"%d dG = n/a\n",seqlen);
    bpairs = hashtbl_get(consensus,node->data);
    if (!bpairs)
      fprintf(stderr,"no bpairs found\n");
    for (bpnode = hashtbl_getkeys(bpairs); bpnode; bpnode = bpnode->next) {
      pair = mystrdup(bpnode->data);
      //printf("processing pair %s\n",pair);
      i = strtok(pair,blank);
      j = strtok(NULL,blank);
      hashtbl_insert(temp,i,j);
      hashtbl_insert(temp,j,i);

    }
    for (m = 0; m < seqlen; m++) {
      sprintf(key,"%d",m+1);
      if ((j = hashtbl_get(temp,key)) )
	fprintf(fp,"\t%d %c\t%d   %d   %s   %d\n",m+1,set->seq[m],m,m+2,j,m+1);
      else
	fprintf(fp,"\t%d %c\t%d   %d   0   %d\n",m+1,set->seq[m],m,m+2,m+1);
    }
    hashtbl_destroy(temp);
    fclose(fp);
  }
  hashtbl_destroy(consensus);
  return 0;
}

void free_Set(Set* set) {
  free(set);
}
