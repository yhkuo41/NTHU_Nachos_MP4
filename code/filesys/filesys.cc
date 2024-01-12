// filesys.cc
//	Routines to manage the overall operation of the file system.
//	Implements routines to map from textual file names to files.
//
//	Each file in the file system has:
//	   A file header, stored in a sector on disk
//		(the size of the file header data structure is arranged
//		to be precisely the size of 1 disk sector)
//	   A number of data blocks
//	   An entry in the file system directory
//
// 	The file system consists of several data structures:
//	   A bitmap of free disk sectors (cf. bitmap.h)
//	   A directory of file names and file headers
//
//      Both the bitmap and the directory are represented as normal
//	files.  Their file headers are located in specific sectors
//	(sector 0 and sector 1), so that the file system can find them
//	on bootup.
//
//	The file system assumes that the bitmap and directory files are
//	kept "open" continuously while Nachos is running.
//
//	For those operations (such as Create, Remove) that modify the
//	directory and/or bitmap, if the operation succeeds, the changes
//	are written immediately back to disk (the two files are kept
//	open during all this time).  If the operation fails, and we have
//	modified part of the directory and/or bitmap, we simply discard
//	the changed version, without writing it back to disk.
//
// 	Our implementation at this point has the following restrictions:
//
//	   there is no synchronization for concurrent accesses
//	   files have a fixed size, set when the file is created
//	   files cannot be bigger than about 3KB in size
//	   there is no hierarchical directory structure, and only a limited
//	     number of files can be added to the system
//	   there is no attempt to make the system robust to failures
//	    (if Nachos exits in the middle of an operation that modifies
//	    the file system, it may corrupt the disk)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.
#ifndef FILESYS_STUB

#include "copyright.h"
#include "debug.h"
#include "disk.h"
#include "pbitmap.h"
#include "directory.h"
#include "filehdr.h"
#include "filesys.h"

// Sectors containing the file headers for the bitmap of free sectors,
// and the directory of files.  These file headers are placed in well-known
// sectors, so that they can be located on boot-up.
#define FreeMapSector 0
#define DirectorySector 1

// Initial file sizes for the bitmap and directory; until the file system
// supports extensible files, the directory size sets the maximum number
// of files that can be loaded onto the disk.
#define FreeMapFileSize (NumSectors / BitsInByte)
#define DirectoryFileSize (sizeof(DirectoryEntry) * NumDirEntries)

//----------------------------------------------------------------------
// FileSystem::FileSystem
// 	Initialize the file system.  If format = TRUE, the disk has
//	nothing on it, and we need to initialize the disk to contain
//	an empty directory, and a bitmap of free sectors (with almost but
//	not all of the sectors marked as free).
//
//	If format = FALSE, we just have to open the files
//	representing the bitmap and the directory.
//
//	"format" -- should we initialize the disk?
//----------------------------------------------------------------------
FileSystem::FileSystem(bool format)
{
    DEBUG(dbgFile, "Initializing the file system.");
    if (format)
    {
        PersistentBitmap *freeMap = new PersistentBitmap(NumSectors);
        Directory *directory = new Directory(NumDirEntries);
        FileHeader *mapHdr = new FileHeader;
        FileHeader *dirHdr = new FileHeader;

        DEBUG(dbgFile, "Formatting the file system.");

        // First, allocate space for FileHeaders for the directory and bitmap
        // (make sure no one else grabs these!)
        freeMap->Mark(FreeMapSector);
        freeMap->Mark(DirectorySector);

        // Second, allocate space for the data blocks containing the contents
        // of the directory and bitmap files.  There better be enough space!
        ASSERT(mapHdr->Allocate(freeMap, FreeMapFileSize));
        ASSERT(dirHdr->Allocate(freeMap, DirectoryFileSize));
        // Flush the bitmap and directory FileHeaders back to disk
        // We need to do this before we can "Open" the file, since open
        // reads the file header off of disk (and currently the disk has garbage
        // on it!).
        DEBUG(dbgFile, "Writing headers back to disk.");
        mapHdr->WriteBack(FreeMapSector);
        dirHdr->WriteBack(DirectorySector);

        // OK to open the bitmap and directory files now
        // The file system operations assume these two files are left open
        // while Nachos is running.
        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);

        // Once we have the files "open", we can write the initial version
        // of each file back to disk.  The directory at this point is completely
        // empty; but the bitmap has been changed to reflect the fact that
        // sectors on the disk have been allocated for the file headers and
        // to hold the file data for the directory and bitmap.
        DEBUG(dbgFile, "Writing bitmap and directory back to disk.");
        freeMap->WriteBack(freeMapFile); // flush changes to disk
        directory->WriteBack(directoryFile);

        if (debug->IsEnabled('f'))
        {
            freeMap->Print();
            directory->Print();
        }
        delete freeMap;
        delete directory;
        delete mapHdr;
        delete dirHdr;
    }
    else
    {
        // if we are not formatting the disk, just open the files representing
        // the bitmap and directory; these are left open while Nachos is running
        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
    }

    for (int i = 0; i < FILE_OPEN_LIMIT; i++)
    {
        OpenFileTable[i] = NULL;
    }
}

//----------------------------------------------------------------------
// MP4 mod tag
// FileSystem::~FileSystem
//----------------------------------------------------------------------
FileSystem::~FileSystem()
{
    delete freeMapFile;
    delete directoryFile;
}

//----------------------------------------------------------------------
// FileSystem::Create
// 	Create a file in the Nachos file system (similar to UNIX create).
//	Since we can't increase the size of files dynamically, we have
//	to give Create the initial size of the file.
//
//	The steps to create a file are:
//	  Make sure the file doesn't already exist
//        Allocate a sector for the file header
// 	  Allocate space on disk for the data blocks for the file
//	  Add the name to the directory
//	  Store the new file header on disk
//	  Flush the changes to the bitmap and the directory back to disk
//
//	Return TRUE if everything goes ok, otherwise, return FALSE.
//
// 	Create fails if:
//   		file is already in directory
//	 	no free space for file header
//	 	no free entry for file in directory
//	 	no free space for data blocks for the file
//
// 	Note that this implementation assumes there is no concurrent access
//	to the file system!
//
//	"name" -- name of file to be created
//	"initialSize" -- size of file to be created
//----------------------------------------------------------------------
bool FileSystem::Create(char *name, int initialSize)
{
    return createFileOrDir(name, FILE, initialSize);
}

//----------------------------------------------------------------------
// FileSystem::Open
// 	Open a file for reading and writing.
//	To open a file:
//	  Find the location of the file's header, using the directory
//	  Bring the header into memory
//
//	"name" -- the text name of the file to be opened
//----------------------------------------------------------------------
OpenFile *FileSystem::Open(char *name)
{
    FileFinder finder = FileFinder();
    finder.find(name, FILE, directoryFile);
    if (!finder.exist)
    {
        finder.find(name, DIR, directoryFile);
    }
    ASSERT(finder.exist && finder.fhSector >= 0);
    return new OpenFile(finder.fhSector);
}

//----------------------------------------------------------------------
// FileSystem::Remove
// 	Delete a file from the file system.  This requires:
//	    Remove it from the directory
//	    Delete the space for its header
//	    Delete the space for its data blocks
//	    Write changes to directory, bitmap back to disk
//
//	Return TRUE if the file was deleted, FALSE if the file wasn't
//	in the file system.
//
//	"name" -- the text name of the file to be removed
//----------------------------------------------------------------------
bool FileSystem::Remove(const char *name, bool recursive)
{
    if (recursive)
    {
        return recursivelyRemove(name);
    }
    FileFinder finder = FileFinder();
    finder.find(name, FILE, directoryFile);
    if (!finder.exist)
    {
        return FALSE;
    }
    PersistentBitmap *freeMap = new PersistentBitmap(freeMapFile, NumSectors);
    returnSectorsToFreeMap(finder.fhSector, freeMap);
    OpenFile *openPfh = new OpenFile(finder.pFhSector);
    Directory *pDir = new Directory(NumDirEntries);
    pDir->FetchFrom(openPfh);
    ASSERT(pDir->Remove(finder.filename.c_str(), FILE));

    freeMap->WriteBack(freeMapFile);
    pDir->WriteBack(openPfh);
    DEBUG(dbgMp4, "remove " << name << " (single file)");
    delete freeMap;
    delete openPfh;
    delete pDir;
    return TRUE;
}

bool FileSystem::recursivelyRemove(const char *name)
{
    FileFinder finder = FileFinder();
    finder.find(name, DIR, directoryFile);
    if (!finder.exist)
    {
        return Remove(name, FALSE); // remove single file
    };
    // remove all files/dirs in this dir
    PersistentBitmap *freeMap = new PersistentBitmap(freeMapFile, NumSectors);
    OpenFile *openfh = new OpenFile(finder.fhSector);
    Directory *dir = new Directory(NumDirEntries);
    dir->FetchFrom(openfh);
    ASSERT(dir->RemoveAll(freeMap));
    dir->WriteBack(openfh);
    freeMap->WriteBack(freeMapFile);
    delete freeMap;
    delete openfh;
    delete dir;
    return TRUE;
}

//----------------------------------------------------------------------
// FileSystem::List
// 	List all the files in the file system directory.
//----------------------------------------------------------------------
void FileSystem::List(char *name, bool recursive)
{
    FileFinder finder = FileFinder();
    finder.find(name, DIR, directoryFile);
    // file not exist
    if (!finder.exist)
    {
        return;
    }
    ASSERT(finder.fhSector != INVALID_SECTOR);
    OpenFile *f = new OpenFile(finder.fhSector);
    Directory *dir = new Directory(NumDirEntries);
    dir->FetchFrom(f);
    delete f;
    if (recursive)
    {
        dir->RecursivelyList(0);
    }
    else
    {
        dir->List();
    }
    delete dir;
}

//----------------------------------------------------------------------
// FileSystem::Print
// 	Print everything about the file system:
//	  the contents of the bitmap
//	  the contents of the directory
//	  for each file in the directory,
//	      the contents of the file header
//	      the data in the file
//----------------------------------------------------------------------
void FileSystem::Print()
{
    FileHeader *bitHdr = new FileHeader;
    FileHeader *dirHdr = new FileHeader;
    PersistentBitmap *freeMap = new PersistentBitmap(freeMapFile, NumSectors);
    Directory *directory = new Directory(NumDirEntries);

    printf("Bit map file header:\n");
    bitHdr->FetchFrom(FreeMapSector);
    bitHdr->Print();

    printf("Directory file header:\n");
    dirHdr->FetchFrom(DirectorySector);
    dirHdr->Print();

    freeMap->Print();

    directory->FetchFrom(directoryFile);
    directory->Print();

    delete bitHdr;
    delete dirHdr;
    delete freeMap;
    delete directory;
}

void FileSystem::PrintHeader(char *name)
{
    FileFinder finder = FileFinder();
    finder.find(name, DIR, directoryFile);
    if (!finder.exist)
    {
        finder.find(name, FILE, directoryFile);
    }
    ASSERT(finder.fhSector != INVALID_SECTOR);
    FileHeader *hdr = new FileHeader();
    hdr->FetchFrom(finder.fhSector);
    hdr->Print(FALSE);
    delete hdr;
}

OpenFileId FileSystem::OpenAFile(char *name)
{
    OpenFileId id;
    for (id = 0; id < 20; ++id)
    {
        if (OpenFileTable[id] == NULL)
        {
            break;
        }
    }
    // exceed the opened file limit
    if (id == 20)
    {
        return -1;
    }
    OpenFileTable[id] = Open(name);
    return id;
}

int FileSystem::WriteFile_(char *buffer, int size, OpenFileId id)
{
    if (buffer != NULL && size >= 0 && isValidFileId(id))
    {
        return OpenFileTable[id]->Write(buffer, size);
    }
    return -1;
}

int FileSystem::ReadFile(char *buffer, int size, OpenFileId id)
{
    if (buffer != NULL && size >= 0 && isValidFileId(id))
    {
        return OpenFileTable[id]->Read(buffer, size);
    }
    return -1;
}

int FileSystem::CloseFile(OpenFileId id)
{
    if (isValidFileId(id))
    {
        delete OpenFileTable[id];
        OpenFileTable[id] = NULL;
        return 1;
    }
    return -1;
}

bool FileSystem::isValidFileId(OpenFileId id)
{
    return id >= 0 && id < FILE_OPEN_LIMIT && OpenFileTable[id] != NULL;
}

bool FileSystem::Mkdir(char *name)
{
    return createFileOrDir(name, DIR, -1);
}

bool FileSystem::createFileOrDir(char *name, bool isDir, int initialSize)
{
    // 1. find the parent dir
    FileFinder finder = FileFinder();
    finder.find(name, isDir, directoryFile);
    if (finder.exist)
    {
        DEBUG(dbgMp4, "File or dir exist, cannot create: " << name);
        return FALSE;
    }
    ASSERT(finder.pFhSector != INVALID_SECTOR); // parent dir should exist

    // 2. add file header to the parent dir
    PersistentBitmap *freeMap = new PersistentBitmap(freeMapFile, NumSectors);
    int sector = freeMap->FindAndSet();
    ASSERT(sector >= 0);
    OpenFile *openPfh = new OpenFile(finder.pFhSector);
    Directory *pDir = new Directory(NumDirEntries);
    pDir->FetchFrom(openPfh);

    ASSERT(pDir->Add(finder.filename.c_str(), sector, isDir)) // assume always have space

    // 3. allocate data blocks
    int size = isDir ? NumDirEntries * sizeof(DirectoryEntry) : initialSize;
    ASSERT(size >= 0);
    FileHeader *fh = new FileHeader();
    ASSERT(fh->Allocate(freeMap, size));

    // 4. write back
    fh->WriteBack(sector);
    pDir->WriteBack(openPfh);
    freeMap->WriteBack(freeMapFile);
    if (isDir)
    {
        Directory *dir = new Directory(NumDirEntries);
        OpenFile *openFh = new OpenFile(sector);
        dir->WriteBack(openFh);
        delete dir;
        delete openFh;
    }
    delete freeMap;
    delete openPfh;
    delete pDir;
    delete fh;
    return TRUE;
}

void FileSystem::returnSectorsToFreeMap(int fhSector, PersistentBitmap *freeMap)
{
    ASSERT(freeMap->Test(fhSector));
    freeMap->Clear(fhSector); // return header sector
    FileHeader *fh = new FileHeader();
    fh->FetchFrom(fhSector);
    fh->Deallocate(freeMap); // return data sectors
    delete fh;
}

FileFinder::FileFinder() : exist(FALSE), pFhSector(INVALID_SECTOR), fhSector(INVALID_SECTOR) {}

vector<string> FileFinder::splitPath(const char *str, bool &exceedPathLenLimit)
{
    vector<string> result;
    string token;
    int i = 0;
    while (*str)
    {
        if (*str == '/')
        {
            result.push_back(token);
            token.clear();
        }
        else
        {
            token += *str;
        }
        ++str;
        ++i;
    }
    exceedPathLenLimit = i >= PATH_NAME_MAX_LEN;
    result.push_back(token);
    return result;
}

void FileFinder::find(const char *name, bool isDir, OpenFile *root)
{
    bool exceedPathLenLimit = FALSE;
    vector<string> path = splitPath(name, exceedPathLenLimit);
    filename = path.back();
    if (exceedPathLenLimit)
    {
        return;
    }
    // the file is the root dir
    if (!strcmp(name, "/"))
    {
        ASSERT(isDir);
        exist = true;
        fhSector = DirectorySector;
        return;
    }
    OpenFile *openPfh = root; // open parent file header
    Directory *pDir = new Directory(NumDirEntries);
    int pathSz = static_cast<int>(path.size());
    for (int i = 1; i < pathSz; ++i)
    {
        pDir->FetchFrom(openPfh);
        // set pfh sector to previous fh sector
        if (i == 1)
        {
            pFhSector = DirectorySector;
        }
        else
        {
            pFhSector = fhSector;
        }
        // find dir until the leaf
        fhSector = pDir->Find(path[i].c_str(), (i == pathSz - 1 ? isDir : DIR));
        if (fhSector == INVALID_SECTOR)
        {
            deleteOpenFile(openPfh, root);
            delete pDir;
            return;
        }
        deleteOpenFile(openPfh, root);
        openPfh = new OpenFile(fhSector);
    }
    exist = true;
    deleteOpenFile(openPfh, root);
    delete pDir;
}

void FileFinder::deleteOpenFile(OpenFile *openfile, OpenFile *root)
{
    if (openfile != NULL && openfile != root)
    {
        delete openfile;
    }
}
#endif // FILESYS_STUB
