#include "rvm.h"
#include <iostream>
#include <map>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <stdio.h>

using namespace std;

#define DEBUG 1

#ifdef DEBUG
#define PRINT_DEBUG(M, ...) printf("%s\n", M)
#else
#define PRINT_DEBUG(M, ...)
#endif

rvm_t rvm_init(const char *directory)
{
	struct stat sb;
	if (stat(directory, &sb) == 0 && S_ISDIR(sb.st_mode))
	{
	    PRINT_DEBUG("DIRECTORY EXISTS. WHAT NOW?");
	}
	else
	{
		char *cmd = new char(strlen(directory) + 7);
		strcpy(cmd, "mkdir ");
		strcat(cmd, directory);
		PRINT_DEBUG(cmd);
		system(cmd);
	}

	rvm_t rvm = new rvm_data(directory);	
	return rvm;
}
void *rvm_map(rvm_t rvm, const char *segname, int size_to_create)
{
	map<string, segment_t>::iterator it;
	string temp = segname;
	it = rvm->segment_map.find(temp);
	if(it == rvm->segment_map.end())				// Segment doesn't exist. Create and add it
	{
		cout<<"Segment doesn't exist"<<endl;
		//segment_t seg()
	}
	else										// Segment exists
	{
		if(it->second.is_mapped)				// Segment is already mapped. Return error
		{
			cout<<"Is already mapped"<<endl;
			return NULL;
		}
		// Add code here - segment exists but is not mapped
	}
	return NULL;
}
void rvm_unmap(rvm_t rvm, void *segbase);
void rvm_destroy(rvm_t rvm, const char *segname);
trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases);
void rvm_about_to_modify(trans_t tid, void *segbase, int offset, int size);
void rvm_commit_trans(trans_t tid);
void rvm_abort_trans(trans_t tid);
void rvm_truncate_log(rvm_t rvm);

int main()
{
	rvm_t rvm = rvm_init("hi");
	cout<<"RVM INITIALIZED:"<<rvm->path<<endl;
	rvm_map(rvm, "hi1", 100);
	char *blah = new char(10);
	segment_t seg((void*) blah, 10);
	seg.is_mapped = 1;
	rvm->segment_map["hi1"] = seg;
	// // cout<<"Size is "<<rvm.segment_map.size()<<endl;
	cout<<rvm->segment_map["hi1"].address<<endl;
	rvm_map(rvm, "hi1", 10);
	return 0;
}