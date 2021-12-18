
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen_lex_base.h"
#include "codegen_lex_inspect.h"
#include "compiler_utils.h"

static inline
void Advance(inspect_lexer *Lexer)
{
    ++Lexer->At;
    ++Lexer->NextColumn;
}

static
void EatIgnoredCharacters(inspect_lexer *Lexer)
{
    for (;;)
    {
        char C = *Lexer->At;
        
        if (IsWhitespace(C))
        {
            if (C == '\n')
            {
                Lexer->NextLine++;
                Lexer->NextColumn = 0;
            }
            
            Advance(Lexer);
            continue;
        }
        
        // Single line comment
        if (C == '/' && Lexer->At[1] != '\0' && Lexer->At[1] == '/')
        {
            while (*Lexer->At != '\n')
            {
                if (Lexer->At == '\0') return;
                
                Advance(Lexer);
            }
            
            ++Lexer->NextLine;
            Advance(Lexer);
        }
        
        // Block comment
        if (C == '/' && Lexer->At[1] != '\0' && Lexer->At[1] == '*')
        {
            for (;;)
            {
                if (Lexer->At[0] == '\0') return;
                if (Lexer->At[0] == '*')
                {
                    if (Lexer->At[1] == '\0') return;
                    if (Lexer->At[1] == '/')
                    {
                        Lexer->At += 2;
                        break;
                    }
                }
                
                if (*Lexer->At == '\n')
                {
                    ++Lexer->NextLine;
                }
                
                Advance(Lexer);
            }
        }
        
        break;
    }
}

static constexpr char *KeywordStruct = "struct";
static constexpr char *KeywordEnum = "enum";
static constexpr char *KeywordDeclareType = "declare_type";
static constexpr char *KeywordImport = "import";
static constexpr char *KeywordDeclareAttribute = "declare_attribute";
static constexpr char *KeywordAliasAttribute = "alias_attribute";

static
bool SetKeyword(inspect_token *Token)
{
    if (ConstexprStrlen(KeywordStruct) == Token->Length &&
        strncmp(Token->Text, KeywordStruct, Token->Length) == 0)
    {
        Token->Type = ITokenType_Struct;
    }
    else if (ConstexprStrlen(KeywordEnum) == Token->Length &&
             strncmp(Token->Text, KeywordEnum, Token->Length) == 0)
    {
        Token->Type = ITokenType_Enum;
    }
    else if (ConstexprStrlen(KeywordDeclareType) == Token->Length &&
             strncmp(Token->Text, KeywordDeclareType, Token->Length) == 0)
    {
        Token->Type = ITokenType_DeclareType;
    }
    else if (ConstexprStrlen(KeywordImport) == Token->Length &&
             strncmp(Token->Text, KeywordImport, Token->Length) == 0)
    {
        Token->Type = ITokenType_Import;
    }
    else if (ConstexprStrlen(KeywordDeclareAttribute) == Token->Length &&
             strncmp(Token->Text, KeywordDeclareAttribute, Token->Length) == 0)
    {
        Token->Type = ITokenType_DeclareAttribute;
    }
    else if (ConstexprStrlen(KeywordAliasAttribute) == Token->Length &&
             strncmp(Token->Text, KeywordAliasAttribute, Token->Length) == 0)
    {
        Token->Type = ITokenType_AliasAttribute;
    }
    else
    {
        return false;
    }
    
    return true;
}

static
inspect_token NextIdentifierOrNumber(inspect_lexer *Lexer)
{
    inspect_token Token;
    Token.Text = Lexer->At;
    
    bool IsNumber = true;
    
    char *Begin = Lexer->At;
    for(;;)
    {
        switch(*Lexer->At)
        {
            case '.':
            case ',':
            case '(':
            case ')':
            case '{':
            case '}':
            case '[':
            case ']':
            case '<':
            case '>':
            case '\'':
            case '"':
            case '+':
            case '-':
            case '*':
            case '/':
            case '#':
            case '!':
            case '?':
            case '~':
            case '%':
            case '&':
            case '|':
            case ':':
            case ';':
            case '\0':
            case '\r':
            case '\n':
            case '\t':
            case '=':
            case ' ':
            {
                Token.Length = (size_t)(Lexer->At - Begin);
                if (IsNumber)
                {
                    Token.Type = ITokenType_Number;
                }
                else if (!SetKeyword(&Token))
                {
                    Token.Type = ITokenType_Identifier;
                }
                
                return Token;
            }
            
            default:
            {
                if (IsNumber && !IsNumeric(*Lexer->At))
                {
                    IsNumber = false;
                }
                
                Advance(Lexer);
                
                break;
            }
        }
    }
}

bool MoveToEndOfString(inspect_lexer *Lexer)
{
    while(char c = *(Lexer->At++))
    {
        if (c == '\"')
        {
            return true;
        }
    }
    
    return false;
}

#if 0
static inline
bool ContainsCharacter(char C, char *Characters, int Count)
{
    for (int I = 0; I < Count; ++I)
    {
        if (C == Characters[I])
        {
            return true;
        }
    }
    
    return false;
}
#endif

inspect_token NextToken(inspect_lexer *Lexer)
{
    EatIgnoredCharacters(Lexer);
    
    Lexer->Line = Lexer->NextLine;
    Lexer->Column = Lexer->NextColumn;
    
    inspect_token Token;
    Token.Text = Lexer->At;
    Token.Type = ITokenType_Unknown;
    Token.Length = 1;
    
    switch(*Lexer->At)
    {
        case '.': Token.Type = ITokenType_Dot; break;
        case ',': Token.Type = ITokenType_Comma; break;
        case '(': Token.Type = ITokenType_LeftParen; break;
        case ')': Token.Type = ITokenType_RightParen; break;
        case '{': Token.Type = ITokenType_LeftCurly; break;
        case '}': Token.Type = ITokenType_RightCurly; break;
        case '[': Token.Type = ITokenType_LeftSquare; break;
        case ']': Token.Type = ITokenType_RightSquare; break;
        case '<': Token.Type = ITokenType_LeftAngle; break;
        case '>': Token.Type = ITokenType_RightAngle; break;
        case '\'': Token.Type = ITokenType_SingleQuote; break;
        case '+': Token.Type = ITokenType_Plus; break;
        case '-': Token.Type = ITokenType_Minus; break;
        case '*': Token.Type = ITokenType_Asterisk; break;
        case '/': Token.Type = ITokenType_ForwardSlash; break;
        case '#': Token.Type = ITokenType_Pound; break;
        case '!': Token.Type = ITokenType_Exclamation; break;
        case '?': Token.Type = ITokenType_Question; break;
        case '~': Token.Type = ITokenType_Tilde; break;
        case '%': Token.Type = ITokenType_Percent; break;
        case '&': Token.Type = ITokenType_Ampersand; break;
        case '|': Token.Type = ITokenType_Pipe; break;
        case ':': Token.Type = ITokenType_Colon; break;
        case ';': Token.Type = ITokenType_SemiColon; break;
        case '=': Token.Type = ITokenType_Equals; break;
        case '\0': Token.Type = ITokenType_End; break;
    }
    
    if (Token.Type != ITokenType_Unknown)
    {
        Advance(Lexer);
        return Token;
    }
    
    if (*Lexer->At == '\"')
    {
        char *StringBegin = ++Lexer->At;
        if (!MoveToEndOfString(Lexer))
        {
            Token.Type = ITokenType_IncompleteString;
            Token.Length = (size_t)(Lexer->At - Token.Text);
            return Token;
        }
        else
        {
            Token.Type = ITokenType_String;
            Token.Text = StringBegin;
            Token.Length = (size_t)(Lexer->At - StringBegin - 1);
            return Token;
        }
    }
    
    // complex inspect_tokens.
    return NextIdentifierOrNumber(Lexer);
}

bool CreateLexerInternal(char *Filename, inspect_lexer *Result)
{
    char *File = ReadEntireFileAndTerminate(Filename);
    
    if (!File)
    {
        return false;
    }
    
    Result->Directory = GetDirectory(Filename);
    Result->Begin = File;
    Result->At = File;
    Result->Filename = Filename;
    Result->NextLine = 1;
    Result->NextColumn = 1;
    Result->Line = -1;
    Result->Column = -1;
    
    return true;
}

bool CreateLexer(const char *Filename, inspect_lexer *Result)
{
    return CreateLexerInternal(strdup(Filename), Result);
}

bool CreateLexer(char *Filename, inspect_lexer *Result)
{
    return CreateLexerInternal(Filename, Result);
}

bool CreateLexer(char *Filename, size_t size, inspect_lexer *Result)
{
    char *FilenameBuffer = (char *)malloc(size + 1);
    strncpy(FilenameBuffer, Filename, size);
    FilenameBuffer[size] = '\0';
    return CreateLexerInternal(FilenameBuffer, Result);
}

void FreeLexer(inspect_lexer *Lexer)
{
    free(Lexer->Begin);
    free(Lexer->Filename);
    free(Lexer->Directory);
}
