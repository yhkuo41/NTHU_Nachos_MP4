// filehdr.cc
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector,
//
//      Unlike in a real system, we do not keep track of file permissions,
//	ownership, last modification date, etc., in the file header.
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "filehdr.h"
#include "debug.h"
#include "synchdisk.h"
#include "main.h"

//----------------------------------------------------------------------
// MP4 mod tag
// FileHeader::FileHeader
//	There is no need to initialize a fileheader,
//	since all the information should be initialized by Allocate or FetchFrom.
//	The purpose of this function is to keep valgrind happy.
//----------------------------------------------------------------------
FileHeader::FileHeader() : numBytes(-1), numSectors(-1), startSector(INVALID_SECTOR), endSector(INVALID_SECTOR) {}

//----------------------------------------------------------------------
// MP4 mod tag
// FileHeader::~FileHeader
//	Currently, there is not need to do anything in destructor function.
//	However, if you decide to add some "in-core" data in header
//	Always remember to deallocate their space or you will leak memory
//----------------------------------------------------------------------
FileHeader::~FileHeader() {}

//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the bit map of free disk sectors
//----------------------------------------------------------------------
bool FileHeader::Allocate(PersistentBitmap *freeMap, int fileSize)
{
	numBytes = fileSize;
	numSectors = divRoundUp(fileSize, SectorSize);
	if (freeMap->NumClear() < numSectors)
	{
		return FALSE; // not enough space
	}
	ASSERT(dataBlocks.empty() && startSector == INVALID_SECTOR && endSector == INVALID_SECTOR);
	for (int i = 0; i < numSectors; ++i)
	{
		int freeBlock = freeMap->FindAndSet();
		// since we checked that there was enough free space, we expect this to succeed
		ASSERT(freeBlock >= 0);
		if (!i) // first
		{
			startSector = freeBlock;
		}
		else // others
		{
			dataBlocks.back().next = freeBlock;
		}
		dataBlocks.push_back(DataBlock());
		endSector = freeBlock;
	}
	return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------
void FileHeader::Deallocate(PersistentBitmap *freeMap)
{
	ASSERT(static_cast<int>(dataBlocks.size()) == numSectors);
	for (int i = 0, next = startSector; i < numSectors; ++i)
	{
		ASSERT(next >= 0 && freeMap->Test(next)); // ought to be marked!
		freeMap->Clear(next);
		next = dataBlocks[i].next;
	}
	startSector = INVALID_SECTOR;
	endSector = INVALID_SECTOR;
	dataBlocks.clear();
}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk.
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------
void FileHeader::FetchFrom(int sector)
{
	ASSERT(dataBlocks.empty() && startSector == INVALID_SECTOR && endSector == INVALID_SECTOR);
	int buf[SectorSize / sizeof(int)];
	kernel->synchDisk->ReadSector(sector, (char *)buf);
	numBytes = buf[0];
	numSectors = buf[1];
	startSector = buf[2];
	endSector = buf[3];
	// rebuild in-core data blocks
	for (int i = 0, cur = startSector; i < numSectors; ++i)
	{
		kernel->synchDisk->ReadSector(cur, (char *)buf);
		DataBlock db = DataBlock(buf[NUM_DATA]);
		for (int i = 0; i < NUM_DATA; ++i)
		{
			db.data[i] = buf[i];
		}
		dataBlocks.push_back(db);
		cur = db.next;
	}
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk.
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------
void FileHeader::WriteBack(int sector)
{
	int buf[SectorSize / sizeof(int)] = {numBytes, numSectors, startSector, endSector};
	kernel->synchDisk->WriteSector(sector, (char *)buf);
	for (int i = 0, cur = startSector; i < numSectors; ++i)
	{
		for (int j = 0; j < NUM_DATA; ++j)
		{
			buf[j] = dataBlocks[i].data[j];
		}
		buf[NUM_DATA] = dataBlocks[i].next;
		kernel->synchDisk->WriteSector(cur, (char *)buf);
		cur = dataBlocks[i].next;
	}
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------
int FileHeader::ByteToSector(int offset)
{
	int i = (offset / SectorSize) - 1;
	if (i == -1)
	{
		return startSector;
	}
	return dataBlocks[i].next;
}

//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------
int FileHeader::FileLength()
{
	return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::Print
// 	Print the contents of the file header, and the contents of all
//	the data blocks pointed to by the file header.
//----------------------------------------------------------------------
void FileHeader::Print()
{
	int i, j, k, cur;
	char *data = new char[SectorSize];
	cout << "FileHeader contents:" << endl
		 << "1. File size: " << numBytes << " bytes (" << numSectors << " sectors)" << endl
		 << "2. FileHeader size in disk: " << (sizeof(FileHeader) - sizeof(dataBlocks)) << " bytes" << endl
		 << "3. FileHeader size in memory: " << (sizeof(FileHeader) + sizeof(DataBlock) * numSectors) << " bytes" << endl
		 << "4. Data blocks: " << endl;
	for (i = 0, cur = startSector; i < numSectors; i++)
	{
		ASSERT(cur != INVALID_SECTOR);
		cout << cur << " ";
		cur = dataBlocks[i].next;
	}
	printf("\nFile contents:\n");
	for (i = k = 0, cur = startSector; i < numSectors; i++)
	{
		kernel->synchDisk->ReadSector(cur, data);
		for (j = 0; (j < DATA_SIZE) && (k < numBytes); j++, k++)
		{
			if ('\040' <= data[j] && data[j] <= '\176') // isprint(data[j])
			{
				printf("%c", data[j]);
			}
			else
			{
				printf("\\%x", (unsigned char)data[j]);
			}
		}
		printf("\n");
		cur = dataBlocks[i].next;
	}
	delete[] data;
}
