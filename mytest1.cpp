/* basic.c - test that basic persistency works */

#include "rvm.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <iostream>

#define TEST_STRING "hello, world"
#define OFFSET2 100

using namespace std;

/* proc1 writes some data, commits it, then exits */
void proc1() 
{
     // cout<<"at start\n";
     rvm_t rvm;
     trans_t trans;
     char* segs[1];
     
     rvm = rvm_init("rvm_segments");
     rvm_destroy(rvm, "testseg");
     segs[0] = (char *) rvm_map(rvm, "testseg", 100);

     
     trans = rvm_begin_trans(rvm, 1, (void **) segs);
     
     rvm_about_to_modify(trans, segs[0], 0, 100);
     sprintf(segs[0], TEST_STRING);
     rvm_commit_trans(trans);

     rvm_unmap(rvm, segs[0]);

     segs[0] = (char *) rvm_map(rvm, "testseg", 200);
     // cout<<segs[0]<<endl;
     trans = rvm_begin_trans(rvm, 1, (void **) segs);
     rvm_about_to_modify(trans, segs[0], OFFSET2, 100);
     sprintf(segs[0]+OFFSET2, "asdasd");
     // cout<<segs[0]+100<<endl;
     rvm_commit_trans(trans);
     // cout<<"in proc 1 "<< mapped_segments["testseg"]<<endl;

     exit(0);
}


/* proc2 opens the segments and reads from them */
void proc2() 
{
     char* segs[1];
     rvm_t rvm;
     // cout<<"In proc2"<<endl;
     rvm = rvm_init("rvm_segments");
     // cout<<"in proc 2 "<< mapped_segments["testseg"]<<endl;
     segs[0] = (char *) rvm_map(rvm, "testseg", 200);
     if(segs[0] == NULL)
     {
     	cout<<"Not mapped"<<endl;
     	exit(0);
     }
     if(strcmp(segs[0], TEST_STRING)) {
	  printf("ERROR: first hello not present\n");
	  exit(2);
     }
      if(strcmp(segs[0]+OFFSET2, "asdasd")) {
	  printf("ERROR: second hello not present\n");
       cout<<(char*)segs[0]+OFFSET2<<endl;
	  exit(2);
     }

     printf("OK\n");
     exit(0);
}


int main(int argc, char **argv)
{
     int pid;

     pid = fork();
     if(pid < 0) {
	  perror("fork");
	  exit(2);
     }
     if(pid == 0) {
	  proc1();
	  exit(0);
     }

     waitpid(pid, NULL, 0);

     proc2();

     return 0;
}