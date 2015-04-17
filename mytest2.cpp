/* basic.c - test that basic persistency works */

#include "rvm.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <iostream>

#define TEST_STRING "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"


using namespace std;

int main(int argc, char **argv)
{
     rvm_t rvm;
     trans_t trans;
     char* segs[1];
     rvm = rvm_init("rvm_segments");
     rvm_destroy(rvm, "testseg");
     
     for(int i=0;i<50;i++){
          cout<<"map follows\n";
          segs[0]= (char *) rvm_map(rvm, "testseg", 2*i+3);
          cout<<"begin transaction follows\n";
          trans = rvm_begin_trans(rvm, 1, (void **) segs);     
          cout<<"about to modify follows\n";
          rvm_about_to_modify(trans, segs[0], 2*i, 2);
          cout<<"sprintf (write to memory) follows\n";
          sprintf(segs[0] + 2*i, "xx");
          cout<<"at i="<<i<<endl<<segs[0]<<endl;
          cout<<"commit follows "<<trans<<endl;
          rvm_commit_trans(trans);
          cout<<"unmap follows\n";
          rvm_unmap(rvm, segs[0]);
     }

     // cout<<"in proc 1 "<< mapped_segs[0]ments["testsegs[0]"]<<endl;
     segs[0] = (char *) rvm_map(rvm, "testseg", 100);
     if(segs[0]== NULL)
     {
          cout<<"Not mapped"<<endl;
          exit(0);
     }
     if(strcmp(segs[0], TEST_STRING)) {
       printf("ERROR: Failed comparison\nFound this:\n");
       cout<<segs[0]<<endl;
       exit(2);
     }

     printf("OK\n");
     exit(0);

     abort();

     return 0;
}