#include "rvm.h"
#include <iostream>
#include <map>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <cstdio>
#include <time.h>
#include <fstream>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

using namespace std;

#ifdef DEBUG
#define PRINT_DEBUG(M, ...) printf("%s\n", M)
#else
#define PRINT_DEBUG(M, ...)
#endif

#define MAX_TRANS_ID 1000000

map<trans_t, trans_data> trans_map;

void apply_log_for_segment(rvm_t rvm, string segname)
{
	string seg_file_path = rvm->path + "/" + segname;
	string log_file_path = seg_file_path + ".log";
	int seg_file, log_file;
	int offset, size;
	void *value;
	int size_of_int = sizeof(int);
	seg_file = open(seg_file_path.c_str(), O_RDWR | O_CREAT, 0755);
	if(seg_file == -1)
	{
		PRINT_DEBUG("Error opening segment file");
		return;
	}
	log_file = open(log_file_path.c_str(), O_RDWR | O_CREAT, 0755);
	if(log_file == -1)
	{
		close(seg_file);
		PRINT_DEBUG("Error opening log file");
		return;
	}
	int file_size = lseek(log_file, 0, SEEK_END);
	if(file_size <= 0)
		return;
	lseek(log_file, 0, 0);
	while(lseek(log_file, 0, SEEK_CUR) < file_size)
	{
		read(log_file, &offset, size_of_int);
		read(log_file, &size, size_of_int);
		value = operator new(size);
		read(log_file, value, size);
		lseek(seg_file, offset, 0);
		write(seg_file, value, size);
		operator delete(value);
	}
	close(seg_file);
	close(log_file);
	string cmd = "rm " + log_file_path;
	system(cmd.c_str());
	cmd = "touch " + log_file_path;
	system(cmd.c_str());
}

rvm_t rvm_init(const char *directory)
{
	struct stat sb;
	string dir = directory;
	if (stat(directory, &sb) == 0 && S_ISDIR(sb.st_mode))
	{
	    PRINT_DEBUG("DIRECTORY EXISTS.");	    
	}
	else
	{
		char *cmd = new char(strlen(directory) + 7);
		strcpy(cmd, "mkdir ");
		strcat(cmd, directory);
		PRINT_DEBUG(cmd);
		system(cmd);
		delete(cmd);
	}
	rvm_t rvm = new rvm_data(directory);	
	return rvm;
}

void *rvm_map(rvm_t rvm, const char *segname, int size_to_create)
{
	string temp = string(segname);

	int length = rvm->path.length() + strlen(segname) + 1;
	char *file_path = new char(length);

	map<string, segment_t>::iterator it;
	it = rvm->segment_map.find(temp);
	if(it != rvm->segment_map.end())
	{
		if(it->second.is_mapped == 1)
			return NULL;
	}
	apply_log_for_segment(rvm, temp);

	strcpy(file_path, rvm->path.c_str());
	strcat(file_path, "/");
	strcat(file_path, segname);


	int file, size, result;
	file = open(file_path, O_RDWR | O_CREAT, 0755);
    if(file == -1)
    {   
	    PRINT_DEBUG("Error opening file");
	    return NULL;
	}
	size = lseek(file, 0, SEEK_END);
    if (size == -1) 
    {
        close(file);
        PRINT_DEBUG("Error getting size");
        return NULL;
    }

    if (size < size_to_create)						// Size of the segment is less than the size to be created. Extend it
    {
        lseek(file, size_to_create - 1, SEEK_SET);
        result = write(file, "\0", 1);
        if (result == -1) {           
            close(file);
            PRINT_DEBUG("Error extending file");
            return NULL;
        }           
    }

	segment_t seg;
	seg.segname = temp;
	seg.size = size_to_create;
	seg.is_mapped = 1;
	seg.address = operator new(size_to_create);
	lseek(file, 0, 0);     
    result = read(file, seg.address, size_to_create);  
    if(result == -1)
    {
    	PRINT_DEBUG("Error reading file");
    	close(file);
    	return NULL;
    }
    delete(file_path);
	rvm->segment_map[temp] = seg;

	close(file);
	return rvm->segment_map[temp].address;
}

void rvm_unmap(rvm_t rvm, void *segbase)
{
	map<string, segment_t>::iterator it;
	string segname;
	for(it = rvm->segment_map.begin(); it != rvm->segment_map.end(); it++)
	{
		if(it->second.address == segbase)
			break;
	}
	if(it->second.address != segbase)		// There is no such segment
	{
		PRINT_DEBUG("The segment to be unmapped does not exist");
		return;
	}
	segname = it->first;
	if(it->second.is_mapped == 0)			// This process has not mapped the segment
	{
		PRINT_DEBUG("The segment to be unmapped has not been mapped before.");
		return;
	}
	if(it->second.being_used)				// The segment is being modified
	{
		PRINT_DEBUG("The segment to be unmapped is in use.");
		return;
	}

	operator delete(it->second.address);
	rvm->segment_map.erase(it);
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
	if(it->second.is_mapped == 1)			// The segment is currently mapped. 
	{
		PRINT_DEBUG("The segment to be destroyed is being used.");
		return;
	}

	// Erase the backing store and log also
	operator delete(it->second.address);
	rvm->segment_map.erase(it);
	string cmd = "rm " + rvm->path + "/" + temp;
	system(cmd.c_str());
	cmd = cmd + ".log";
	system(cmd.c_str());
}

trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases)
{

	map<string, segment_t>::iterator it;
	trans_data trans;
	trans.rvm = rvm;
	int found;
	for(int i = 0; i < numsegs; i++)
	{
		found = 0;
		for(it = rvm->segment_map.begin(); it != rvm->segment_map.end(); it++)
		{			
			if(it->second.address == segbases[i])
			{
				found = 1;
				if(it->second.being_used == 1)					// This segment is being used by another transaction
				{
					PRINT_DEBUG("This segment is being used by another transaction");
					return -1;
				}
				if(it->second.is_mapped == 0)					// This segment has not been mapped
				{
					PRINT_DEBUG("This segment has not been mapped.");
					return -1;
				}
				trans.segments[segbases[i]] = &(it->second);
			}
		}
		if(found == 0)						// This segment does not exist
		{
			PRINT_DEBUG("This segment does not exist");
			return -1;
		}
	}
	map<void*, segment_t*>::iterator it_seg;
	for(it_seg = trans.segments.begin(); it_seg != trans.segments.end(); it_seg++)
	{
		trans.segments[it_seg->first]->being_used = 1;		// Indicate that the segment is being used						
	}

	srand(time(NULL));
	trans_t trans_id =  rand() % MAX_TRANS_ID;					// Generate new transaction ID
	while(trans_map.count(trans_id) == 1)
		trans_id = rand() % MAX_TRANS_ID;
	trans_map[trans_id] = trans;
	return trans_id;
}

void rvm_about_to_modify(trans_t tid, void *segbase, int offset, int size)
{
	if(trans_map.count(tid) == 0)							// Invalid tid
	{
		PRINT_DEBUG("Invalid transaction id");
		return;
	}

	if(trans_map[tid].segments.count(segbase) == 0)					// Segment not selected for this transaction
	{
		PRINT_DEBUG("Segment not selected for this transaction");
		return;
	}
	if(offset + size > trans_map[tid].segments[segbase]->size)		// Outside the range of the segment
	{
		PRINT_DEBUG("Outside the range of the segment");
		return;
	}
	undo_record_t undo_record;
	undo_record.offset = offset;
	undo_record.size = size;
	undo_record.backup = operator new(size);
	memcpy(undo_record.backup, (char*) segbase + offset, size);
	trans_map[tid].undo_records[segbase].push_front(undo_record);	
}

void rvm_commit_trans(trans_t tid)
{
	if(trans_map.count(tid) == 0)							// Invalid tid
	{
		PRINT_DEBUG("Invalid transaction id");
		return;
	}

	string path = trans_map[tid].rvm->path;
	map<void*, list<undo_record_t> >::iterator it;
	void *segbase;
	undo_record_t undo_record;
	map<void*, segment_t*>::iterator it_seg;
	string segname;
	string log_file_path;
	int log_file;

	for(it = trans_map[tid].undo_records.begin(); it != trans_map[tid].undo_records.end(); it++)
	{
		segbase = it->first;
		it_seg = trans_map[tid].segments.find(segbase);
		segname = it_seg->second->segname;
		log_file_path = path + "/" + segname + ".log";
		log_file = open(log_file_path.c_str(), O_RDWR | O_CREAT, 0755);
		if(log_file == -1)
		{
			PRINT_DEBUG("Error opening log file");
			continue;
		}
		lseek(log_file, 0, SEEK_END);

		while(!it->second.empty())							// Going through the undo records
		{			
			undo_record = it->second.front();
			write(log_file, &(undo_record.offset), sizeof(int));
			write(log_file, &(undo_record.size), sizeof(int));
			write(log_file, (char*) it_seg->second->address + undo_record.offset, undo_record.size);
			operator delete(undo_record.backup);
			it->second.pop_front();
		}
		it_seg->second->being_used = 0;						// Marking the segment as no longer in use
		trans_map[tid].segments.erase(it_seg);
		trans_map[tid].undo_records.erase(it);
		close(log_file);
	}
	// Remove transaction info
	trans_map.erase(tid);
}

void rvm_abort_trans(trans_t tid)
{
	PRINT_DEBUG("In abort transaction");
	if(trans_map.count(tid) == 0)							// Invalid tid
	{
		PRINT_DEBUG("Invalid transaction id");
		return;
	}

	map<void*, list<undo_record_t> >::iterator it;
	void *segbase;
	undo_record_t undo_record;
	for(it = trans_map[tid].undo_records.begin(); it != trans_map[tid].undo_records.end(); it++)
	{
		segbase = it->first;
		while(!it->second.empty())
		{			
			undo_record = it->second.front();			
			memcpy((char*)segbase+undo_record.offset, undo_record.backup, undo_record.size);
			operator delete(undo_record.backup);
			it->second.pop_front();
		}
		trans_map[tid].undo_records.erase(it);
	}

	trans_data trans = trans_map[tid];						// Mark all segments unmapped
	map<void*, segment_t*>::iterator it_seg;
	for(it_seg = trans.segments.begin(); it_seg != trans.segments.end(); it_seg++)
	{
		trans.segments[it_seg->first]->being_used = 0;		// Indicate that the segment is no longer being used							
	}
	
	trans_map.erase(tid);									// Remove transaction info
}


void rvm_truncate_log(rvm_t rvm)
{
	DIR *dp;
    struct dirent *dirp;

    if((dp = opendir(rvm->path.c_str())) == NULL) {
        return;
    }
    string file_name;
    while ((dirp = readdir(dp)) != NULL) 
    {
        file_name = string(dirp->d_name);
        if(file_name.length() > 4 && file_name.compare(file_name.length()-4, 4, ".log") == 0)
        {
        	apply_log_for_segment(rvm, file_name.substr(0, file_name.length()-4));
        }
    }
    closedir(dp);
}
