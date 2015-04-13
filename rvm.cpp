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
		PRINT_DEBUG("Segment doesn't exist. Creating new.");
		segment_t seg;
		seg.address = operator new(size_to_create);
		seg.size = size_to_create;
		seg.is_mapped = 1;
		rvm->segment_map[temp] = seg;
	}
	else											// Segment exists
	{
		if(it->second.is_mapped)					// Segment is already mapped. Return error
		{
			PRINT_DEBUG("Segment is already mapped. Returning NULL");
			return NULL;
		}
		// Add code here - segment exists but is not mapped. Maybe create an undo record?
		if(it->second.size < size_to_create)
		{
			// If the segment exists but is shorter than size_to_create, then extend it until it is long enough
			
			//copy the old value. 
			PRINT_DEBUG("Changing size");
			operator delete(it->second.address); 
			void *address = operator new(size_to_create);
			it->second.address = address;
			it->second.size = size_to_create;
			it->second.is_mapped = 1;
		}
		else
		{
			// copy the old value. If the memory has been freed on unmapping, then here we will need to reallocate memory
			it->second.is_mapped = 1;
		}
	}
	return rvm->segment_map[temp].address;
}

void rvm_unmap(rvm_t rvm, void *segbase)
{
	map<string, segment_t>::iterator it;
	for(it = rvm->segment_map.begin(); it != rvm->segment_map.end(); it++)
	{
		if(it->second.address == segbase)
			break;
	}
	if(it->second.address != segbase)		// There is no such segment
	{
		// The segment has not been mapped. Abort or something
		PRINT_DEBUG("The segment to be unmapped does not exist");
		return;
	}
	if(it->second.is_mapped == 0)
	{
		PRINT_DEBUG("The segment to be unmapped has not been mapped before.");
		return;
	}
	if(it->second.being_used)				// The segment is being modified
	{
		// The segment is in use. Do something
		PRINT_DEBUG("The segment to be unmapped is in use.");
		return;
	}
	// Should the memory be freed?
	PRINT_DEBUG("Unmapping the segment");
	it->second.is_mapped = 0;
}
void rvm_destroy(rvm_t rvm, const char *segname)
{
	map<string, segment_t>::iterator it;
	string temp = segname;
	it = rvm->segment_map.find(temp);
	if(it == rvm->segment_map.end())		// The segment does not exist
	{
		PRINT_DEBUG("The segment to be deleted does not exist");
		return;
	}
	if(it->second.being_used == 1)			// The segment is in use
	{
		// WHat should be done here?
		PRINT_DEBUG("The segment to be destroyed is being used.");
		return;
	}
	operator delete(it->second.address);
	rvm->segment_map.erase(it);
}

trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases);
void rvm_about_to_modify(trans_t tid, void *segbase, int offset, int size)
{
	// Check if it is mapped
}
void rvm_commit_trans(trans_t tid);
void rvm_abort_trans(trans_t tid);
void rvm_truncate_log(rvm_t rvm);

int main()
{
	rvm_t rvm = rvm_init("hi");
	cout<<"RVM INITIALIZED:"<<rvm->path<<endl;
	char *seg_ch = (char*) rvm_map(rvm, "hi1", 100);
	cout<<rvm->segment_map["hi1"].address<<"\t"<<rvm->segment_map["hi1"].size<<"\t"<<rvm->segment_map["hi1"].is_mapped<<endl;
	printf("%p\n", seg_ch);
	rvm->segment_map["hi1"].is_mapped = 0;
	seg_ch = (char*) rvm_map(rvm, "hi1", 1000);
	cout<<rvm->segment_map["hi1"].address<<"\t"<<rvm->segment_map["hi1"].size<<"\t"<<rvm->segment_map["hi1"].is_mapped<<endl;
	printf("%p\n", seg_ch);
	int *seg_int = (int*) rvm_map(rvm, "hi2", 200);
	float *seg_fl = (float*) rvm_map(rvm, "hi3", 300);
	rvm_unmap(rvm, seg_ch);
	seg_ch = (char*) rvm_map(rvm, "hi1", 100);
	printf("%p\n", seg_ch);
	rvm_destroy(rvm, "hi1");
	seg_ch = (char*) rvm_map(rvm, "hi1", 100);
	printf("%p\n", seg_ch);
	return 0;
}