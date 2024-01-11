// filesys.h
//	Data structures to represent the Nachos file system.
//
//	A file system is a set of files stored on disk, organized
//	into directories.  Operations on the file system have to
//	do with "naming" -- creating, opening, and deleting files,
//	given a textual file name.  Operations on an individual
//	"open" file (read, write, close) are to be found in the OpenFile
//	class (openfile.h).
//
//	We define two separate implementations of the file system.
//	The "STUB" version just re-defines the Nachos file system
//	operations as operations on the native UNIX file system on the machine
//	running the Nachos simulation.
//
//	The other version is a "real" file system, built on top of
//	a disk simulator.  The disk is simulated using the native UNIX
//	file system (in a file named "DISK").
//
//	In the "real" implementation, there are two key data structures used
//	in the file system.  There is a single "root" directory, listing
//	all of the files in the file system; unlike UNIX, the baseline
//	system does not provide a hierarchical directory structure.
//	In addition, there is a bitmap for allocating
//	disk sectors.  Both the root directory and the bitmap are themselves
//	stored as files in the Nachos file system -- this causes an interesting
//	bootstrap problem when the simulated disk is initialized.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#ifndef FS_H
#define FS_H

#include <vector>
#include "copyright.h"
#include "sysdep.h"
#include "openfile.h"
#include "pbitmap.h"

#define PATH_NAME_MAX_LEN 256
#define FILE_OPEN_LIMIT 20

typedef int OpenFileId;

#ifdef FILESYS_STUB // Temporarily implement file system calls as
// calls to UNIX, until the real file system
// implementation is available
class FileSystem
{
public:
	FileSystem()
	{
		for (int i = 0; i < 20; i++)
			fileDescriptorTable[i] = NULL;
	}

	bool Create(char *name)
	{
		int fileDescriptor = OpenForWrite(name);

		if (fileDescriptor == -1)
			return FALSE;
		Close(fileDescriptor);
		return TRUE;
	}

	OpenFile *Open(char *name)
	{
		int fileDescriptor = OpenForReadWrite(name, FALSE);

		if (fileDescriptor == -1)
			return NULL;
		return new OpenFile(fileDescriptor);
	}

	bool Remove(char *name) { return Unlink(name) == 0; }

	OpenFile *fileDescriptorTable[20];
};

#else // FILESYS
class FileFinder
{
	friend class FileSystem;
	// is the file exist?
	bool exist;
	// sector number of the parent dir
	int pFhSector;
	// sector number of the file header
	int fhSector;
	string filename;

	FileFinder();
	static vector<string> splitPath(const char *name, bool &exceedPathLenLimit);
	// find the file by path (name) and set the result to private fields
	void find(const char *name, bool isDir, OpenFile *root);
	void deleteOpenFile(OpenFile *openfile, OpenFile *root);
};

class FileSystem
{
	friend class FileFinder;

public:
	// Initialize the file system. Must be called *after* "synchDisk" has been initialized. If "format",
	// there is nothing on the disk, so initialize the directory and the bitmap of free blocks.
	FileSystem(bool format);
	// MP4 mod tag
	~FileSystem();
	// Create a file (UNIX creat)
	bool Create(char *name, int initialSize);
	// Open a file (UNIX open)
	OpenFile *Open(char *name);
	// This function is used for kernel open system call
	OpenFileId OpenAFile(char *name);
	int WriteFile_(char *buffer, int size, OpenFileId id);
	int ReadFile(char *buffer, int size, OpenFileId id);
	int CloseFile(OpenFileId id);
	// Delete a file (UNIX unlink)
	bool Remove(const char *name, bool recursive);
	// List all the files in the file system
	void List(char *name, bool recursive);
	// List all the files and their contents
	void Print();
	bool Mkdir(char *name);

private:
	// Bit map of free disk blocks, represented as a file
	OpenFile *freeMapFile;
	// "Root" directory -- list of file names, represented as a file
	OpenFile *directoryFile;
	OpenFile *OpenFileTable[FILE_OPEN_LIMIT];
	bool isValidFileId(OpenFileId id);
	/**
	 * @brief Create a File Or Dir
	 *
	 * @param name absolute path
	 * @param isDir is this a dir or a file
	 * @param initialSize file size (will be ignored if this is a dir)
	 * @return true success
	 * @return false fail
	 */
	bool createFileOrDir(char *name, bool isDir, int initialSize);
	bool recursivelyRemove(const char *name);
	// Return data and header sectors to freeMap
	void returnSectorsToFreeMap(int fhSector, PersistentBitmap *freeMap);
};

#endif // FILESYS

#endif // FS_H
