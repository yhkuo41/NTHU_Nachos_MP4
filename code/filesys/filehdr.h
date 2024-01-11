// filehdr.h
//	Data structures for managing a disk file header.
//
//	A file header describes where on disk to find the data in a file,
//	along with other information about the file (for instance, its
//	length, owner, etc.)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef FILEHDR_H
#define FILEHDR_H

#include "disk.h"
#include "pbitmap.h"
#include <vector>
#define INVALID_SECTOR -1
// How many bytes of data can be stored in a data block
const int DATA_SIZE = SectorSize - sizeof(int);
// How many integers (data) can be stored in a data block
const int NUM_DATA = DATA_SIZE / sizeof(int);

class DataBlock
{
	friend class FileHeader;
	int data[NUM_DATA];
	// the sector number of the next data block
	int next;
	DataBlock(int next = INVALID_SECTOR) : next(next) { fill(data, data + NUM_DATA, 0); }
};

// The following class defines the Nachos "file header" (in UNIX terms,
// the "i-node"), describing where on disk to find all of the data in the file.
// The file header is organized as a simple table of pointers to
// data blocks.
//
// The file header data structure can be stored in memory or on disk.
// When it is on disk, it is stored in a single sector -- this means
// that we assume the size of this data structure to be the same
// as one disk sector.  Without indirect addressing, this
// limits the maximum file length to just under 4K bytes.
//
// There is no constructor; rather the file header can be initialized
// by allocating blocks for the file (if it is a new file), or by
// reading it from disk.
class FileHeader
{
public:
	// MP4 mod tag
	// dummy constructor to keep valgrind happy
	FileHeader();
	~FileHeader();
	// Initialize a file header, including allocating space on disk for the file data
	bool Allocate(PersistentBitmap *bitMap, int fileSize);
	// De-allocate this file's data blocks
	void Deallocate(PersistentBitmap *bitMap);
	// Initialize file header from disk
	void FetchFrom(int sectorNumber);
	// Write modifications to file header  back to disk
	void WriteBack(int sectorNumber);
	// Convert a byte offset into the file to the disk sector containing the byte
	int ByteToSector(int offset);
	// Return the length of the file in bytes
	int FileLength();
	// Print the contents of the file.
	void Print();

private:
	//---disk part---//

	// Number of bytes in the file
	int numBytes;
	// Number of data sectors in the file
	int numSectors;
	int startSector;
	int endSector;
	//---disk part---//
	//---in-core part---//

	vector<DataBlock> dataBlocks;
	//---in-core part---//
};
#endif // FILEHDR_H
