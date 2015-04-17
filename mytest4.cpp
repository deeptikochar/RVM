/* basic.c - test that basic persistency works */

#include "rvm.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <iostream>

#define TSTRING11 "hello-11"
#define TSTRING12 "hello-12"
#define TSTRING21 "hello-11"
#define TSTRING22 "hello-22"
#define OFFSET2 100

using namespace std;

/* proc1 writes some data, commits it, then exits */
void proc1() 
{
     // cout<<"at start\n";
     rvm_t rvm;
     rvm_t rvm1;
     trans_t trans;
     char* segs[2];
     char* regs[2];
     
     rvm = rvm_init("rvm_segments");
     rvm1 = rvm_init("test_rvm");
     // rvm_destroy(rvm, "testseg");
     segs[0] = (char *) rvm_map(rvm, "seg1", 100);
     segs[1] = (char *) rvm_map(rvm, "seg2", 100);
     
     trans = rvm_begin_trans(rvm, 2, (void **) segs);
     
     rvm_about_to_modify(trans, segs[0], 0, 100);
     rvm_about_to_modify(trans, segs[1], 0, 100);
     sprintf(segs[0], TSTRING11);
     sprintf(segs[1], TSTRING12);
     // cout<<segs[0]<<endl;
     // cout<<segs[1]<<endl;
     rvm_commit_trans(trans);

     rvm_unmap(rvm, segs[0]);
     rvm_unmap(rvm, segs[1]);

     segs[0] = (char *) rvm_map(rvm, "seg1", 200);
     if(segs[0] == NULL)cout<<"NULL!!\n";
     segs[1] = (char *) rvm_map(rvm, "seg1", 200);
     // cout<<segs[0]<<endl;
     if(segs[1] == NULL)cout<<"NULL!!\n";
     trans = rvm_begin_trans(rvm, 2, (void **) segs);
     
     rvm_about_to_modify(trans, segs[0], OFFSET2, 100);
     rvm_about_to_modify(trans, segs[1], OFFSET2, 100);
     sprintf(segs[0]+OFFSET2, "asdasd");
     sprintf(segs[1]+OFFSET2, "asdasd");
     // cout<<segs[0]+100<<endl;
     rvm_commit_trans(trans);
     // cout<<"in proc 1 "<< mapped_segments["testseg"]<<endl;

     regs[0] = (char *) rvm_map(rvm1, "reg1", 100);
     regs[1] = (char *) rvm_map(rvm1, "reg2", 100);
     
     trans = rvm_begin_trans(rvm1, 2, (void **) regs);
     
     rvm_about_to_modify(trans, regs[0], 0, 100);
     rvm_about_to_modify(trans, regs[1], 0, 100);
     sprintf(regs[0], TSTRING21);
     sprintf(regs[1], TSTRING22);
     // cout<<segs[0]<<endl;
     // cout<<segs[1]<<endl;
     rvm_commit_trans(trans);

     rvm_unmap(rvm1, regs[0]);
     rvm_unmap(rvm1, regs[1]);

     regs[0] = (char *) rvm_map(rvm1, "reg1", 200);
     regs[1] = (char *) rvm_map(rvm1, "reg2", 200);
     // cout<<segs[0]<<endl;
     trans = rvm_begin_trans(rvm1, 2, (void **) regs);
     
     rvm_about_to_modify(trans, regs[0], OFFSET2, 100);
     rvm_about_to_modify(trans, regs[1], OFFSET2, 100);
     sprintf(regs[0]+OFFSET2, "asdasd");
     sprintf(regs[1]+OFFSET2, "asdasd");
     // cout<<segs[0]+100<<endl;
     rvm_commit_trans(trans);

     exit(0);
}


/* proc2 opens the segments and reads from them */
void proc2() 
{
     char* segs[2];
     char* regs[2];
     rvm_t rvm, rvm1;
     // cout<<"In proc2"<<endl;
     rvm = rvm_init("rvm_segments");
     rvm1 = rvm_init("test_rvm");
     // cout<<"in proc 2 "<< mapped_segments["testseg"]<<endl;
     segs[0] = (char *) rvm_map(rvm, "seg1", 200);
     segs[1] = (char *) rvm_map(rvm, "seg2", 200);
     regs[0] = (char *) rvm_map(rvm1, "reg1", 200);
     regs[1] = (char *) rvm_map(rvm1, "reg2", 200);
     
     if(segs[0] == NULL || segs[1] == NULL)
     {
     	cout<<"Not mapped"<<endl;
     	exit(0);
     }
     if(strcmp(segs[0], TSTRING11) || strcmp(segs[1], TSTRING12) || strcmp(regs[0], TSTRING21) || strcmp(regs[1], TSTRING22)) {
	  printf("ERROR: first hello not present\n");
	  exit(2);
     }
      if(strcmp(segs[0]+OFFSET2, "asdasd") || strcmp(segs[1]+OFFSET2, "asdasd") || strcmp(regs[0]+OFFSET2, "asdasd") || strcmp(regs[1]+OFFSET2, "asdasd")) {
	  printf("ERROR: second hello not present\n");
       // cout<<(char*)segs[1]+OFFSET2<<endl;
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