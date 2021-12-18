#pragma once

inline
void PrintLocation(int Line, int Column, const char *Filename)
{
    printf("%s:%i:%i: ", Filename, Line, Column);
}

#if 0
inline
void PrintLocation(int Line, int Column, char *Filename, size_t Length)
{
    printf("%.*s:%i:%i: ", (int)Length, Filename, Line, Column);
}
#endif