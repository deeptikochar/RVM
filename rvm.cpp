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

#define DEBUG 1

#ifdef DEBUG
#define PRINT_DEBUG(M, ...) printf("%s\n", M)
#else
#define PRINT_DEBUG(M, ...)
#endif

#define MAX_TRANS_ID 1000000

map<trans_t, trans_data> trans_map;
map<string, int> mapped_segments;

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

	// go through log file. For each of them do the following steps:
	// read(log_file, offset, sizeof(int));
	// read(log_file, size, sizeof(int));
	// value = operator new(size);
	// read(log_file, value, size);
	// seek to that offset in segment file
	// write(seg_file, value, size);
	// operator delete(value);

	// at the end, clear the log file

}

rvm_t rvm_init(const char *directory)
{
	struct stat sb;
	string dir = directory;
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

	string temp = segname;
	if(mapped_segments.count(temp))					// The segment is already mapped - error
	{
		PRINT_DEBUG("Segment is already mapped");
		return NULL;
	}
	int length = rvm->path.length() + strlen(segname) + 1;
	char *file_path = new char(length);
	char *log_file_path = new char(length + 4);

	strcpy(file_path, rvm->path.c_str());
	strcat(file_path, "/");
	strcat(file_path, segname);
	strcpy(log_file_path, file_path);
	strcat(log_file_path, ".log");

	int file, size, result, log_file;
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
    cout<<size<<endl;
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

    log_file = open(log_file_path, O_RDWR | O_CREAT, 0755);		// Create log file
    if(log_file == -1)
    {
    	close(file);
    	close(log_file);
    	PRINT_DEBUG("Error creating log file");
    	return NULL;
    }

    // Do we truncate the log for this segment?

	segment_t seg;
	seg.segname = temp;
	seg.size = size_to_create;
	seg.is_mapped = 1;
	seg.address = operator new(size_to_create);
	lseek(file, 0, SEEK_SET);     
    result = read(file, seg.address, size_to_create);  
    if(result == -1)
    {
    	PRINT_DEBUG("Error reading file");
    	close(file);
    	close(log_file);
    	return NULL;
    }

	rvm->segment_map[temp] = seg;
	mapped_segments[temp] = 1;

	close(file);
	close(log_file);
	PRINT_DEBUG("EXITING MAP");
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
		// The segment has not been mapped. Abort or something
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
		// The segment is in use. What to do here?
		PRINT_DEBUG("The segment to be unmapped is in use.");
		return;
	}
	// Should the memory be freed?
	PRINT_DEBUG("Unmapping the segment");

	operator delete(it->second.address);
	rvm->segment_map.erase(it);
	mapped_segments[segname] = 0;

	// what to do with log files?
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
		// WHat should be done here? This function should not be called on a segment that is currently mapped
		PRINT_DEBUG("The segment to be destroyed is being used.");
		return;
	}
	// Erase the backing store also
	// delete the file?
	operator delete(it->second.address);
	rvm->segment_map.erase(it);
	mapped_segments.erase(temp);
	string cmd = "rm " + rvm->path + "/" + temp;
	system(cmd.c_str());
	cmd = cmd + ".log";
	system(cmd.c_str());
}

trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases)
{
	/* begin a transaction that will modify the segments listed in segbases. 
	If any of the specified segments is already being modified by a transaction, then the call should fail and return (trans_t) -1. 
	Note that trant_t needs to be able to be typecasted to an integer type. */
	// PRINT_DEBUG("In begin transaction");
	map<string, segment_t>::iterator it;
	trans_data trans;
	trans.rvm = rvm;
	int found;
	for(int i = 0; i < numsegs; i++)
	{
		found = 0;
		// printf("Looking for segment %p \n", segbases[i]);
		for(it = rvm->segment_map.begin(); it != rvm->segment_map.end(); it++)
		{			
			if(it->second.address == segbases[i])
			{
				// printf("Found segment %p \n", segbases[i]);
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
				// cout<<"Inserted: "<<trans.segments[segbases[i]]->address<<endl;
				// cout<<"Inserted: "<<trans.segments[segbases[i]]->being_used<<endl;
			}
		}
		if(found == 0)						// This segment does not exist
		{
			PRINT_DEBUG("This segment does not exist");
			return -1;
		}
	}
	// PRINT_DEBUG("All segments OK!");
	map<void*, segment_t*>::iterator it_seg;
	for(it_seg = trans.segments.begin(); it_seg != trans.segments.end(); it_seg++)
	{
		// cout<<trans.segments[it_seg->first]->address<<endl;
		trans.segments[it_seg->first]->being_used = 1;		// Indicate that the segment is being used
							
	}
	// cout<<"here"<<endl;
	srand(time(NULL));
	trans_t trans_id =  rand() % MAX_TRANS_ID;					// Generate new transaction ID
	while(trans_map.count(trans_id) == 1)
		trans_id = rand() % MAX_TRANS_ID;
	trans_map[trans_id] = trans;
	return trans_id;
}

void rvm_about_to_modify(trans_t tid, void *segbase, int offset, int size)
{
	PRINT_DEBUG("In about to modify");
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
	memcpy(undo_record.backup, segbase + offset, size);
	// cout<<"backup is "<<(char*)undo_record.backup<<endl;
	trans_map[tid].undo_records[segbase].push_front(undo_record);
	cout<<(char*)trans_map[tid].undo_records[segbase].front().backup<<endl;	
}

void rvm_commit_trans(trans_t tid)
{
	/*commit all changes that have been made within the specified transaction. 
	When the call returns, then enough information should have been saved to disk so that, 
	even if the program crashes, the changes will be seen by the program when it restarts.*/

	if(trans_map.count(tid) == 0)							// Invalid tid
	{
		PRINT_DEBUG("Invalid transaction id");
		return;
	}

	// Push to disk
 
 	// What happens if a segment has already been unmapped?


						
	
	// for(it_seg = trans.segments.begin(); it_seg != trans.segments.end(); it_seg++)
	// {
	// 	//cout<<trans.segments[it_seg->first]->address<<endl;
	// 	log_file = it->second->segname + ".log";
	// 	file = open(log_file.c_str(), O_RDWR | O_CREAT, 0755);
	// 	if(file == -1)
	// 	{
	// 		PRINT_DEBUG("Error opening log file");
	// 		continue;
	// 	}
	// 	lseek(file, 0, SEEK_END);

	// 	// now go through the undo_records - store the offset, size and current segment value in the log file. also free the undo records

		
	// 	trans.segments[it_seg->first]->being_used = 0;		// Indicate that the segment is no longer being used
	// 	close(file);
	// }

	// Remove and free undo records
	string path = trans_map[tid].rvm->path;
	map<void*, list<undo_record_t> >::iterator it;
	void *segbase;
	undo_record_t undo_record;
	map<void*, segment_t*>::iterator it_seg;
	string segname;
	string log_file_path;
	int log_file;
	int result;
	for(it = trans_map[tid].undo_records.begin(); it != trans_map[tid].undo_records.end(); it++)
	{
		segbase = it->first;
		it_seg = trans_map[tid].segments.find(segbase);
		segname = it_seg->second->segname;
		log_file_path = path + "/" + segname;
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
			write(log_file, &undo_record.offset, sizeof(int));
			write(log_file, &undo_record.size, sizeof(int));
			write(log_file, it_seg->second->address + undo_record.offset, undo_record.size);

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
		// Check if this segment has been unmapped? If it has then?
		segbase = it->first;
		cout<<"segbase is "<<segbase;
		while(!it->second.empty())
		{			
			undo_record = it->second.front();			
			memcpy(segbase+undo_record.offset, undo_record.backup, undo_record.size);
			operator delete(undo_record.backup);
			it->second.pop_front();
		}
		trans_map[tid].undo_records.erase(it);
	}

	trans_data trans = trans_map[tid];						// Mark all segments unmapped
	map<void*, segment_t*>::iterator it_seg;
	for(it_seg = trans.segments.begin(); it_seg != trans.segments.end(); it_seg++)
	{
		cout<<trans.segments[it_seg->first]->address<<endl;
		trans.segments[it_seg->first]->being_used = 0;		// Indicate that the segment is no longer being used
							
	}
	
	trans_map.erase(tid);									// Remove transaction info
	cout<<"Exiting abort transaction"<<endl;

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
        	cout<<file_name<<endl;
        	apply_log_for_segment(rvm, file_name.substr(0, file_name.length()-4));
        }
    }
    closedir(dp);
}

int main()
{
	rvm_t rvm = rvm_init("hi");
	// cout<<"RVM INITIALIZED:"<<rvm->path<<endl;
	// char *seg_ch = (char*) rvm_map(rvm, "hi1", 200);
	// cout<<rvm->segment_map["hi1"].address<<"\t"<<rvm->segment_map["hi1"].size<<"\t"<<rvm->segment_map["hi1"].is_mapped<<endl;
	// // printf("%p\n", seg_ch);
	// // rvm->segment_map["hi1"].is_mapped = 0;
	// seg_ch = (char*) rvm_map(rvm, "hi1", 1000);
// 	// cout<<rvm->segment_map["hi1"].address<<"\t"<<rvm->segment_map["hi1"].size<<"\t"<<rvm->segment_map["hi1"].is_mapped<<endl;
// 	// printf("%p\n", seg_ch);
// 	// int *seg_int = (int*) rvm_map(rvm, "hi2", 200);
// 	// float *seg_fl = (float*) rvm_map(rvm, "hi3", 300);
// 	// rvm_unmap(rvm, seg_ch);
// 	// seg_ch = (char*) rvm_map(rvm, "hi1", 100);
// 	// printf("%p\n", seg_ch);
// 	// rvm_destroy(rvm, "hi1");
// 	// seg_ch = (char*) rvm_map(rvm, "hi1", 100);
// 	// printf("%p\n", seg_ch);

	char *seg_tr[3];
	seg_tr[0] = (char*) rvm_map(rvm, "SEG0", 100);
	seg_tr[1] = (char*) rvm_map(rvm, "SEG1", 200);
	seg_tr[2] = (char*) rvm_map(rvm, "SEG2", 300);
	printf("Segments %p , %p , %p \n", (void*) seg_tr[0], (void*) seg_tr[1], (void*) seg_tr[2]);
	trans_t tid = rvm_begin_trans(rvm, 3, (void**) seg_tr);
	cout<<tid<<endl;
// 	strcpy(seg_tr[0], "abcde");
// 	cout<<"seg_tr[0] is "<<seg_tr[0]<<endl;
// 	rvm_about_to_modify(tid, seg_tr[0], 1, 4);
// 	strcpy(seg_tr[0], "blahblah");
// 	cout<<"seg_tr[0] is "<<seg_tr[0]<<endl;
// 	rvm_commit_trans(tid);
// 	// cout<<"seg_tr[0] is "<<seg_tr[0]<<endl;
// 	cout<<"Transaction "<<trans_map.count(tid)<<endl;
// 	trans_t tid2 = rvm_begin_trans(rvm, 2, (void**) seg_tr);	
// 	cout<<tid2<<endl;

	return 0;
}