# RVM

Team members   
*Deepti Kochar
*Aditya Kadur  

COMPILATION INSTRUCTIONS :

To compile the library, type   make
To compile the test files use g++ <filename> -L<path to library> -lrvm
Note: Please make sure you use g++, not gcc for test files

ABOUT THE IMPLEMENTATION :

1. Whenever an rvm_about_to_modify function is called, an undo_record is created in memory which stores the offset, size and backup of actual data in this region. 
2. When a commit is requested, the library looks at all the undo records and makes an entry for each in the log file (1 log file per segment) in the format specified below,  and removes the undo_records from disk.  
3. The format of an entry in the log file is - 4 bytes for offset, 4 bytes for size and size number of bytes for modified data (not the backup).
4. Apart from the log file, there is a mapping of the segment present on disk (from moment of creation until explicitly destroyed).
5. When a truncate_log request is issued, the log file entries are applied onto the segment on disk in oldest-latest fashion (reading from start of log file till the end). This operation leaves the log file completely empty and the segment on disk up to date.
6. When a new mapping is done, we first check for existing segments of the provided name on disk. If found, we call apply_log_for_segment to bring the segment to the latest commit state and then bring it into memory.
7. When truncate_log is called, it does apply_log_for_segment for all the segments in this rvm.
8. When rvm_abort_transaction is called, we go through all the undo records for that transaction in oldest-latest fashion, recopying the backed up data to the corresponding offsets in their segments.



