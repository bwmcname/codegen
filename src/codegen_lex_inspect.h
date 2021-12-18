#pragma once

struct inspect_lexer
{
    char *Directory;
    char *Filename;
    
    char *Begin;
    char *At;
    
    // The line and the column of the position of the "At" pointer.
    int NextLine;
    int NextColumn;
    
    // The line and the column of the last given token (This is probably what you want!)
    int Line;
    int Column;
};

enum inspect_token_type
{
    ITokenType_End,
    ITokenType_Unknown,
    
    ITokenType_Dot,
    ITokenType_Comma,
    ITokenType_LeftParen,
    ITokenType_RightParen,
    ITokenType_LeftCurly,
    ITokenType_RightCurly,
    ITokenType_LeftSquare,
    ITokenType_RightSquare,
    ITokenType_RightAngle,
    ITokenType_LeftAngle,
    ITokenType_SingleQuote,
    ITokenType_DoubleQuote,
    ITokenType_Plus,
    ITokenType_Minus,
    ITokenType_Asterisk,
    ITokenType_ForwardSlash,
    ITokenType_Pound,
    ITokenType_Exclamation,
    ITokenType_Question,
    ITokenType_Tilde,
    ITokenType_Percent,
    ITokenType_Ampersand,
    ITokenType_Pipe,
    ITokenType_Colon,
    ITokenType_SemiColon,
    ITokenType_Equals,
    
    ITokenType_Identifier,
    ITokenType_String,
    ITokenType_Number,
    ITokenType_Struct,
    ITokenType_Enum,
    ITokenType_DeclareType,
    ITokenType_Import,
    ITokenType_DeclareAttribute,
    ITokenType_AliasAttribute,
    
    // Error Types
    ITokenType_IncompleteString,
};

struct inspect_token
{
    char *Text;
    size_t Length;
    inspect_token_type Type;
};

inspect_token NextToken(inspect_lexer *Lexer);
bool CreateLexer(const char *Filename, inspect_lexer *Result);
bool CreateLexer(char *Filename, size_t length, inspect_lexer *Result);
bool CreateLexer(char *Filename, inspect_lexer *Result);
void FreeLexer(inspect_lexer *Lexer);
