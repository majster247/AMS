#ifndef __MYOS__DRIVERS__FILESYSTEM_H
#define __MYOS__DRIVERS__FILESYSTEM_H

#include <common/types.h>
using namespace myos::common;

namespace myos
{
    namespace drivers
    {
        namespace Filesystem {
        
            const int MAX_FILES = 100;
            const int MAX_FILENAME_LENGTH = 32;
            const int BLOCK_SIZE = 512;

            // Directory Entry
            struct DirectoryEntry {
                char filename[MAX_FILENAME_LENGTH];
                uint32_t startBlock;
                uint32_t fileSize;
            };

            // Initialize the filesystem (e.g., allocate data structures, open disk file).
            void Initialize();

            // Create a new file with a given name.
            bool CreateFile(const char* filename);

            // Delete a file with a given name.
            bool DeleteFile(const char* filename);

            // Open a file for reading or writing.
            int OpenFile(const char* filename);

            // Close a file.
            void CloseFile(int fileHandle);

            // Read data from an open file.
            int ReadFile(int fileHandle, char* buffer, int length);

            // Write data to an open file.
            int WriteFile(int fileHandle, const char* data, int length);

            // List all files in the filesystem.
            void ListFiles();

            // Uninitialize the filesystem (e.g., release resources, close disk file).
            void Uninitialize();

        }
    }
}

#endif // SIMPLEFS_H
