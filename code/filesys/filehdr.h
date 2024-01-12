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
#define LEVEL_LIMIT 4
const int NUM_DIRECT = (SectorSize - 2 * sizeof(int)) / sizeof(int);
// 3840 bytes = 3.75 KB (30 sectors)
const int MAX_SIZE_L0 = NUM_DIRECT * SectorSize;
// 115200 bytes = 112.5 KB (900 sectors)
const int MAX_SIZE_L1 = NUM_DIRECT * NUM_DIRECT * SectorSize;
// 3456000 bytes = 3375 KB (27000 sectors)
const int MAX_SIZE_L2 = NUM_DIRECT * NUM_DIRECT * NUM_DIRECT * SectorSize;
// 103680000 bytes = 101250 KB = 98.876953125 MB (810000 sectors)
const int MAX_SIZE_L3 = NUM_DIRECT * NUM_DIRECT * NUM_DIRECT * NUM_DIRECT * SectorSize;
const int MAX_SIZE[LEVEL_LIMIT] = {MAX_SIZE_L0, MAX_SIZE_L1, MAX_SIZE_L2, MAX_SIZE_L3};

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
	void Print(bool printContent = TRUE);

private:
	// ====================disk part====================

	// Number of bytes in the file
	int numBytes;
	// Number of data sectors in the file
	int numDataSectors;
	// Disk sector numbers for each data block in the file
	int dataSectors[NUM_DIRECT];
	// ====================disk part====================

	// ====================in-core part====================

	// index: logical sector, value: physical sector
	vector<int> dataSectorMapping;
	FileHeader *children[NUM_DIRECT];
	// ====================in-core part====================
	int whichLv(int fileSize);
	void clear();
	bool Allocate(PersistentBitmap *bitMap, int fileSize, vector<int> &pSectors);
	void FetchFrom(int sectorNumber, vector<int> &pSectors);
};

#endif // FILEHDR_H
