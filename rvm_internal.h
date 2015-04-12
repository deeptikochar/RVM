#include <map>
#include <string>

using namespace std;

typedef struct segment
{
	void *address;
	int size;
	int is_mapped;
	int being_used;	
	segment() {};
	segment(void *seg_address, int seg_size)
	{
		address = seg_address;
		size = seg_size;
		is_mapped = false;
		being_used = false;
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

