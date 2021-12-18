
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

char *ReadEntireFileAndTerminate(const char *Filename)
{
    FILE *File = fopen(Filename, "rb");
    if (File)
    {
        fseek(File, 0, SEEK_END);
        size_t Size = (size_t)ftell(File);
        fseek(File, 0, SEEK_SET);
        
        char *Buffer = (char *)malloc(Size + 1);
        if (1 == fread(Buffer, Size, 1, File))
        {
            Buffer[Size] = '\0';
            return Buffer;
        }
        
        free(Buffer);
        return 0;
    }
    
    return 0;
}

char *GetDirectory(const char *Filename)
{
    int LastSlash = -1;
    for(int I = 0; Filename[I]; ++I)
    {
        if (Filename[I] == '/' ||
            Filename[I] == '\\')
        {
            LastSlash = I;
        }
    }
    
    if (LastSlash == -1)
    {
        char *Empty = (char *)malloc(1);
        *Empty = '\0';
        return Empty;
    }
    
    char *Buffer = (char *)malloc((size_t)(LastSlash + 1));
    strncpy(Buffer, Filename, (size_t)LastSlash);
    Buffer[LastSlash] = '\0';
    return Buffer;
}

char *GetFilename(const char *Path)
{
    int LastSlash = -1;
    for (int I = 0; Path[I]; ++I)
    {
        if (Path[I] == '/' ||
            Path[I] == '\\')
        {
            LastSlash = I;
        }
    }
    
    if (LastSlash == -1)
    {
        return strdup(Path);
    }
    
    const char *Begin = &Path[LastSlash + 1];
    size_t Length = strlen(Begin);
    char *Buffer = (char *)malloc(Length + 1);
    strcpy(Buffer, Begin);
    Buffer[Length] = '\0';
    return Buffer;
}

char *ReplaceExtension(const char *Filename, const char *NewExtension)
{
    int LastPeriod = -1;
    for (int I = 0; Filename[I]; ++I)
    {
        if (Filename[I] == '.')
        {
            LastPeriod = I;
        }
    }
    
    size_t ExtensionBegin;
    if (LastPeriod == -1)
    {
        ExtensionBegin = strlen(Filename);
    }
    else
    {
        ExtensionBegin = (size_t)LastPeriod;
    }
    
    size_t NewExtensionLength = strlen(NewExtension);
    size_t NewFilenameLength = ExtensionBegin + NewExtensionLength;
    char *Buffer = (char *)malloc(NewFilenameLength + 1);
    strncpy(Buffer, Filename, ExtensionBegin);
    strncpy(&Buffer[ExtensionBegin], NewExtension, NewExtensionLength);
    Buffer[NewFilenameLength] = '\0';
    return Buffer;
}

char *AppendToDirectory(const char *Directory, const char *Path)
{
    size_t DirectoryLength = strlen(Directory);
    size_t PathLength = strlen(Path);
    
    // + 1 for added '/'
    size_t FinalLength = DirectoryLength + PathLength + 1;
    char *Buffer = (char *)malloc(FinalLength + 1);
    strcpy(Buffer, Directory);
    Buffer[DirectoryLength] = '/';
    strcpy(&Buffer[DirectoryLength + 1], Path);
    Buffer[FinalLength] = '\0';
    return Buffer;
}
