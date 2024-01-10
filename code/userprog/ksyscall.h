/**************************************************************
 *
 * userprog/ksyscall.h
 *
 * Kernel interface for systemcalls
 *
 * by Marcus Voelp  (c) Universitaet Karlsruhe
 *
 **************************************************************/

#ifndef __USERPROG_KSYSCALL_H__
#define __USERPROG_KSYSCALL_H__

#include "kernel.h"

#include "synchconsole.h"

void SysHalt()
{
	kernel->interrupt->Halt();
}

int SysAdd(int op1, int op2)
{
	return op1 + op2;
}

int SysCreate(char *filename, int initialSize)
{
	// return value
	// 1: success
	// 0: failed
	return kernel->fileSystem->Create(filename, initialSize);
}

/**
 * @brief Open a file
 *
 * @param name file name
 * @return OpenFileId if success, else -1
 */
OpenFileId SysOpen(char *name)
{
	return kernel->fileSystem->OpenAFile(name);
}

/**
 * @brief Write "size" characters from the buffer into the file
 *
 * @param buffer
 * @param size
 * @param id
 * @return int The number of characters actually written to the file. Return -1, if fail to write the file
 */
int SysWrite(char *buffer, int size, OpenFileId id)
{
	return kernel->fileSystem->WriteFile_(buffer, size, id);
}

/**
 * @brief Read "size" characters from the file to the buffer
 *
 * @param buffer
 * @param size
 * @param id
 * @return int  The number of characters actually read from the file. Return -1, if fail to read the file
 */
int SysRead(char *buffer, int size, OpenFileId id)
{
	return kernel->fileSystem->ReadFile(buffer, size, id);
}

/**
 * @brief Close the file with id
 *
 * @param id
 * @return int 1 if success, else -1
 */
int SysClose(OpenFileId id)
{
	return kernel->fileSystem->CloseFile(id);
}

#ifdef FILESYS_STUB
int SysCreate(char *filename)
{
	// return value
	// 1: success
	// 0: failed
	return kernel->interrupt->CreateFile(filename);
}
#endif

#endif /* ! __USERPROG_KSYSCALL_H__ */
