#include <map>
#include <string>
#include <stdio.h>

using namespace std;

typedef struct segment
{
	void *address;
	int size;
	int is_mapped;
	int being_used;	
	segment() 
	{
		is_mapped = 0;
		being_used = 0;
	};
	segment(void *seg_address, int seg_size)
	{
		address = seg_address;
		size = seg_size;
		is_mapped = 0;
		being_used = 0;
	};
} segment_t;

typedef struct rvm
{
	string path;
	map<string, segment_t> segment_map;

	rvm(const char *directory)
	{
		path = directory;
	}

} rvm_data;

typedef rvm_data* rvm_t;

typedef struct 
{
	int i;
} trans_t;

