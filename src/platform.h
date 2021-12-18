
# pragma once

#ifdef _WIN32
#include <windows.h>

#define PLATFORM_IS_DIRECTORY(DirectoryName) Win32IsDirectory(DirectoryName)

inline
bool Win32IsDirectory(const char *DirectoryName)
{
    DWORD Attributes = GetFileAttributes(DirectoryName);
    
    if (Attributes == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }
    
    if (Attributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        return true;
    }
    
    return false;
}

#elif __linux__ // THIS HAS NOT BEEN TESTED!!!
#include <dirent.h>

#define PLATFORM_IS_DIRECTORY(DirectoryName) POSIXIsDirectory(DirectoryName)

inline
bool POSIXIsDirectory(const char *DirectoryName)
{
    DIR *Directory = opendir(DirectoryName);
    if (!Directory)
    {
        return false;
    }
    
    closedir(Directory);
    return false;
}

#endif