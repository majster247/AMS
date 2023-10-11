#include <common/types.h>
#include <cstring> // Include for strcpy and memcpy

using namespace myos;
using namespace myos::common;
using namespace myos::drivers;


namespace Filesystem {

    const std::string DISK_FILENAME = "asmos.disk";

    const int MAX_FILES = 100;

    struct File {
        std::string filename;
        uint32_t startBlock;
        uint32_t fileSize;
    };

    File files[MAX_FILES];
    int numFiles = 0; // Keep track of the number of files in use.

    // Define a constant for block size (adjust as needed).
    const int BLOCK_SIZE = 512;

    char diskData[BLOCK_SIZE * MAX_FILES]; // Simulated disk data.

    bool initialized = false;

    void Initialize();
    void Uninitialize();
    void ReadDirectory();
    void WriteDirectory();
    bool CreateFile(const char* filename);
    bool DeleteFile(const char* filename);
    int OpenFile(const char* filename);
    void CloseFile(int fileHandle);
    int ReadFile(int fileHandle, char* buffer, int length);
    int WriteFile(int fileHandle, const char* data, int length);
    void ListFiles();
}

void Filesystem::Initialize() {
    if (initialized) {
        printf("SimpleFS is already initialized.\n");
        return;
    }

    // Load existing file entries from the simulated disk data.
    ReadDirectory();

    initialized = true;
}

void Filesystem::Uninitialize() {
    if (!initialized) {
        printf("SimpleFS is not initialized.\n");
        return;
    }

    // Save the updated directory to the simulated disk data.
    WriteDirectory();

    initialized = false;
}

void Filesystem::ReadDirectory() {
    // Read the directory entries from the simulated disk data.
    int offset = 0;
    for (int i = 0; i < numFiles; ++i) {
        // Read filename.
        files[i].filename = std::string(&diskData[offset]);
        offset += files[i].filename.size() + 1;

        // Read startBlock.
        std::memcpy(&files[i].startBlock, &diskData[offset], sizeof(uint32_t));
        offset += sizeof(uint32_t);

        // Read fileSize.
        std::memcpy(&files[i].fileSize, &diskData[offset], sizeof(uint32_t));
        offset += sizeof(uint32_t);
    }
}

void Filesystem::WriteDirectory() {
    // Write the directory entries to the simulated disk data.
    int offset = 0;
    for (int i = 0; i < numFiles; ++i) {
        // Write filename.
        std::strcpy(&diskData[offset], files[i].filename.c_str());
        offset += files[i].filename.size() + 1;

        // Write startBlock.
        std::memcpy(&diskData[offset], &files[i].startBlock, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        // Write fileSize.
        std::memcpy(&diskData[offset], &files[i].fileSize, sizeof(uint32_t));
        offset += sizeof(uint32_t);
    }
}

bool Filesystem::CreateFile(const char* filename) {
    if (!initialized) {
        printf("SimpleFS is not initialized.\n");
        return false;
    }

    // Check if the file already exists.
    for (int i = 0; i < numFiles; ++i) {
        if (files[i].filename == filename) {
            printf("File already exists.\n");
            return false;
        }
    }

    if (numFiles >= MAX_FILES) {
        printf("Maximum number of files reached.\n");
        return false;
    }

    // Create a new file entry.
    files[numFiles].filename = filename;
    files[numFiles].startBlock = 0; // Placeholder for the start block.
    files[numFiles].fileSize = 0;
    ++numFiles;

    // Update the directory on the simulated disk data.
    WriteDirectory();

    return true;
}

bool Filesystem::DeleteFile(const char* filename) {
    if (!initialized) {
        printf("SimpleFS is not initialized.\n");
        return false;
    }

    // Find and remove the file entry.
    for (int i = 0; i < numFiles; ++i) {
        if (files[i].filename == filename) {
            // Remove the file by shifting remaining entries.
            for (int j = i; j < numFiles - 1; ++j) {
                files[j] = files[j + 1];
            }
            --numFiles;

            // Update the directory on the simulated disk data.
            WriteDirectory();

            return true;
        }
    }

    printf("File not found.\n");
    return false;
}

int Filesystem::OpenFile(const char* filename) {
    if (!initialized) {
        printf("SimpleFS is not initialized.\n");
        return -1;
    }

    // Find the file entry.
    for (int i = 0; i < numFiles; ++i) {
        if (files[i].filename == filename) {
            // Return a file handle (index in the 'files' array).
            return i;
        }
    }

    printf("File not found.\n");
    return -1;
}

void Filesystem::CloseFile(int fileHandle) {
    // Nothing to do for file closing in this simple example.
}

int Filesystem::ReadFile(int fileHandle, char* buffer, int length) {
    if (!initialized) {
        printf("SimpleFS is not initialized.\n");
        return -1;
    }

    if (fileHandle < 0 || fileHandle >= numFiles) {
        printf("Invalid file handle.\n");
        return -1;
    }

    File& file = files[fileHandle];

    // Read data from the simulated disk (not implemented in this basic example).

    return 0; // Placeholder return value.
}

int Filesystem::WriteFile(int fileHandle, const char* data, int length) {
    if (!initialized) {
        printf("SimpleFS is not initialized.\n");
        return -1;
    }

    if (fileHandle < 0 || fileHandle >= numFiles) {
        printf("Invalid file handle.\n");
        return -1;
    }

    File& file = files[fileHandle];

    // Write data to the simulated disk (not implemented in this basic example).

    return 0; // Placeholder return value.
}

void Filesystem::ListFiles() {
    if (!initialized) {
        printf("SimpleFS is not initialized.\n");
        return;
    }

    printf("Files in the filesystem:\n");
    for (int i = 0; i < numFiles; ++i) {
        printf("%s (%u bytes)\n", files[i].filename.c_str(), files[i].fileSize);
    }
}

int main() {
    Filesystem::Initialize();

    // Test your filesystem functions here.

    Filesystem::Uninitialize();

    return 0;
}
