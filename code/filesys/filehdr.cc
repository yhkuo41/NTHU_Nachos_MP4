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
FileHeader::FileHeader()
{
	memset(children, NULL, sizeof(children));
	clear();
}

//----------------------------------------------------------------------
// MP4 mod tag
// FileHeader::~FileHeader
//	Currently, there is not need to do anything in destructor function.
//	However, if you decide to add some "in-core" data in header
//	Always remember to deallocate their space or you will leak memory
//----------------------------------------------------------------------
FileHeader::~FileHeader()
{
	clear();
}

int FileHeader::whichLv(int fileSize)
{
	for (int i = 0; i < LEVEL_LIMIT; ++i)
	{
		if (fileSize <= MAX_SIZE[i])
		{
			return i;
		}
	}
	ASSERTNOTREACHED(); // don't support file larger than MAX_SIZE_L3
}

void FileHeader::clear()
{
	numBytes = -1;
	numDataSectors = -1;
	memset(dataSectors, INVALID_SECTOR, sizeof(dataSectors));
	dataSectorMapping.resize(0);
	for (int i = 0; i < NUM_DIRECT; ++i)
	{
		if (children[i])
		{
			delete children[i];
			children[i] = NULL;
		}
		else
		{
			break;
		}
	}
}

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
	return Allocate(freeMap, fileSize, this->dataSectorMapping);
}

bool FileHeader::Allocate(PersistentBitmap *freeMap, int fileSize, vector<int> &pSectors)
{
	numBytes = fileSize;
	numDataSectors = divRoundUp(fileSize, SectorSize);
	if (freeMap->NumClear() < numDataSectors)
	{
		return FALSE; // not enough space
	}
	int lv = whichLv(fileSize);
	// DEBUG(dbgMp4, "allocate lv " << lv << " file header which requires " << fileSize << " bytes");
	if (!lv) // direct (original Nachos implementation)
	{
		for (int i = 0; i < numDataSectors; i++)
		{
			dataSectors[i] = freeMap->FindAndSet();
			// since we checked that there was enough free space, we expect this to succeed
			ASSERT(dataSectors[i] >= 0);
			this->dataSectorMapping.push_back(dataSectors[i]);
		}
	}
	else
	{
		int i = 0;
		while (fileSize > 0)
		{
			dataSectors[i] = freeMap->FindAndSet();
			ASSERT(dataSectors[i] >= 0);
			this->children[i] = new FileHeader();
			int subHdrSize = min(fileSize, MAX_SIZE[lv - 1]);
			this->children[i]->Allocate(freeMap, subHdrSize, this->dataSectorMapping); // recursive
			fileSize -= subHdrSize;
			++i;
		}
	}

	if (this->dataSectorMapping != pSectors)
	{
		pSectors.insert(pSectors.end(), this->dataSectorMapping.begin(), this->dataSectorMapping.end());
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
	int lv = whichLv(numBytes);
	if (!lv) // direct (original Nachos implementation)
	{
		for (int i = 0; i < numDataSectors; ++i)
		{
			ASSERT(freeMap->Test((int)dataSectors[i])); // ought to be marked!
			freeMap->Clear((int)dataSectors[i]);
		}
	}
	else
	{
		for (int i = 0; i < NUM_DIRECT; ++i)
		{
			if (dataSectors[i] == INVALID_SECTOR)
			{
				break;
			}
			ASSERT(children[i]);
			children[i]->Deallocate(freeMap);
		}
	}
	clear();
}

void FileHeader::FetchFrom(int sector)
{
	ASSERT(this->dataSectorMapping.empty());
	FetchFrom(sector, this->dataSectorMapping);
}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk.
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------
void FileHeader::FetchFrom(int sector, vector<int> &pSectors)
{
	int buf[SectorSize / sizeof(int)];
	kernel->synchDisk->ReadSector(sector, (char *)buf);
	numBytes = buf[0];
	numDataSectors = buf[1];
	memcpy(dataSectors, buf + 2, sizeof(dataSectors));
	// rebuild in-core part
	int lv = whichLv(numBytes);
	if (!lv) // leaf
	{
		for (int i = 0; i < numDataSectors; i++)
		{
			this->dataSectorMapping.push_back(dataSectors[i]);
		}
	}
	else
	{
		for (int i = 0; i < NUM_DIRECT; ++i)
		{
			if (dataSectors[i] == INVALID_SECTOR)
			{
				break;
			}
			this->children[i] = new FileHeader();
			this->children[i]->FetchFrom(dataSectors[i], this->dataSectorMapping);
		}
	}
	if (this->dataSectorMapping != pSectors)
	{
		pSectors.insert(pSectors.end(), this->dataSectorMapping.begin(), this->dataSectorMapping.end());
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
	int buf[SectorSize / sizeof(int)];
	buf[0] = numBytes;
	buf[1] = numDataSectors;
	memcpy(buf + 2, dataSectors, sizeof(dataSectors));
	kernel->synchDisk->WriteSector(sector, (char *)buf);
	// write disk part
	int lv = whichLv(numBytes);
	if (lv)
	{
		for (int i = 0; i < NUM_DIRECT; ++i)
		{
			if (dataSectors[i] == INVALID_SECTOR)
			{
				break;
			}
			ASSERT(children[i]);
			children[i]->WriteBack(dataSectors[i]);
		}
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
	int logicalSector = offset / SectorSize;
	int sectorsSz = static_cast<int>(dataSectorMapping.size());
	ASSERT(logicalSector >= 0 && logicalSector < sectorsSz && sectorsSz == numDataSectors);
	return dataSectorMapping[logicalSector];
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
void FileHeader::Print(bool printContent)
{
	cout << "FileHeader contents:" << endl
		 << "1. File size: " << numBytes << " bytes (" << numDataSectors << " sectors)" << endl
		 << "2. FileHeader size in disk: " << (sizeof(FileHeader) - sizeof(dataSectorMapping) - sizeof(children)) << " bytes" << endl
		 << "3. FileHeader size in memory: " << (sizeof(FileHeader) + dataSectorMapping.size() * sizeof(int) + sizeof(children)) << " bytes" << endl;
	if (printContent)
	{
		cout << "4. Data blocks: " << endl;
		for (int i = 0; i < numDataSectors; ++i)
		{
			ASSERT(dataSectorMapping[i] != INVALID_SECTOR);
			cout << dataSectorMapping[i] << " ";
		}
		printf("\nFile contents:\n");
	}

	int lv = whichLv(numBytes);
	if (!lv) // leaf
	{
		if (!printContent)
		{
			return;
		}
		char *data = new char[SectorSize];
		for (int i = 0, k = 0; i < numDataSectors; ++i)
		{
			kernel->synchDisk->ReadSector(dataSectors[i], data);
			for (int j = 0; (j < SectorSize) && (k < numBytes); j++, k++)
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
		}
		delete[] data;
	}
	else
	{
		for (int i = 0; i < NUM_DIRECT; ++i)
		{
			if (dataSectors[i] == INVALID_SECTOR)
			{
				break;
			}
			ASSERT(children[i]);
			children[i]->Print(printContent);
		}
	}
}
