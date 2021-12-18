#pragma once

char *ReadEntireFileAndTerminate(const char *Filename);
char *GetDirectory(const char *Filename);
char *GetFilename(const char *Path);
char *ReplaceExtension(const char *Filename, const char *NewExtension);
char *AppendToDirectory(const char *Directory, const char *Path);

inline
bool IsWhitespace(char c)
{
    return c == ' ' ||
        c == '\t' ||
        c == '\n' ||
        c == '\r';
}

inline
bool IsNumeric(char c)
{
    return (c >= '0' && c <= '9');
}
