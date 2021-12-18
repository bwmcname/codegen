
#include <string.h>
#include <stdlib.h>
#include "codegen_lex_base.h"
#include "codegen_lex_write.h"

bool
CreateLexer(write_lexer *Lexer, const char *Filename)
{
    char *Text = ReadEntireFileAndTerminate(Filename);
    if (!Text)
    {
        return false;
    }
    
    Lexer->Begin = Text;
    Lexer->At = Text;
    Lexer->Filename = strdup(Filename);
    Lexer->Mode = Mode_Text;
    Lexer->NextLine = 1;
    Lexer->NextColumn = 1;
    Lexer->Line = -1;
    Lexer->Column = -1;
    Lexer->Flags = 0;
    
    return true;
}

void FreeLexer(write_lexer *Lexer)
{
    free(Lexer->Filename);
    free(Lexer->Begin);
}

static inline
void Advance(write_lexer *Lexer)
{
    Lexer->NextColumn++;
    Lexer->At++;
}

static
void EatIgnoredExpressionCharacters(write_lexer *Lexer)
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
        
        break;
    }
}

static constexpr char *KeywordIf = "if";
static constexpr char *KeywordEnd = "end";
static constexpr char *KeywordFor = "for";
static constexpr char *KeywordForEach = "foreach";
static constexpr char *KeywordIn = "in";
static constexpr char *KeywordIgnoreNewLine = "ignore_new_line";
static constexpr char *KeywordDefine = "define";
static constexpr char *KeywordDefinitions = "definitions";
static constexpr char *KeywordBeginTab = "begin_tab";
static constexpr char *KeywordBreakpoint = "breakpoint";
static constexpr char *KeywordHasAttribute = "has_attribute";

static
bool SetKeyword(write_token *Token)
{
    if (ConstexprStrlen(KeywordIf) == Token->Length &&
        strncmp(Token->Text, KeywordIf, Token->Length) == 0)
    {
        Token->Type = WTokenType_If;
    }
    else if (ConstexprStrlen(KeywordEnd) == Token->Length &&
             strncmp(Token->Text, KeywordEnd, Token->Length) == 0)
    {
        Token->Type = WTokenType_End;
    }
    else if (ConstexprStrlen(KeywordFor) == Token->Length &&
             strncmp(Token->Text, KeywordFor, Token->Length) == 0)
    {
        Token->Type = WTokenType_For;
    }
    else if (ConstexprStrlen(KeywordForEach) == Token->Length &&
             strncmp(Token->Text, KeywordForEach, Token->Length) == 0)
    {
        Token->Type = WTokenType_ForEach;
    }
    else if (ConstexprStrlen(KeywordIn) == Token->Length &&
             strncmp(Token->Text, KeywordIn, Token->Length) == 0)
    {
        Token->Type = WTokenType_In;
    }
    else if (ConstexprStrlen(KeywordIgnoreNewLine) == Token->Length &&
             strncmp(Token->Text, KeywordIgnoreNewLine, Token->Length) == 0)
    {
        Token->Type = WTokenType_IgnoreNewLine;
    }
    else if (ConstexprStrlen(KeywordDefine) == Token->Length &&
             strncmp(Token->Text, KeywordDefine, Token->Length) == 0)
    {
        Token->Type = WTokenType_Define;
    }
    else if (ConstexprStrlen(KeywordDefinitions) == Token->Length &&
             strncmp(Token->Text, KeywordDefinitions, Token->Length) == 0)
    {
        Token->Type = WTokenType_Definitions;
    }
    else if (ConstexprStrlen(KeywordBeginTab) == Token->Length &&
             strncmp(Token->Text, KeywordBeginTab, Token->Length) == 0)
    {
        Token->Type = WTokenType_BeginTab;
    }
    else if (ConstexprStrlen(KeywordBreakpoint) == Token->Length &&
             strncmp(Token->Text, KeywordBreakpoint, Token->Length) == 0)
    {
        Token->Type = WTokenType_Breakpoint;
    }
    else if (ConstexprStrlen(KeywordHasAttribute) == Token->Length &&
             strncmp(Token->Text, KeywordHasAttribute, Token->Length) == 0)
    {
        Token->Type = WTokenType_HasAttribute;
    }
    else
    {
        return false;
    }
    
    return true;
}

static
write_token TextModeGetNext(write_lexer *Lexer);

bool MoveToEndOfString(write_lexer *Lexer)
{
    Advance(Lexer);
    while(*Lexer->At)
    {
        if (*Lexer->At == '\"')
        {
            return true;
        }
        
        Advance(Lexer);
    }
    
    return false;
}

static
write_token ExpressionModeGetNext(write_lexer *Lexer)
{
    Lexer->Line = Lexer->NextLine;
    Lexer->Column = Lexer->NextColumn;
    
    EatIgnoredExpressionCharacters(Lexer);
    write_token Token;
    Token.Type = WTokenType_Unknown;
    
    switch (*Lexer->At)
    {
        case '$':
        {
            Advance(Lexer);
            Lexer->Mode = Mode_Text;
            Lexer->Flags |= WLexerFlag_SilentlyCrossedExpressionBounds;
            return TextModeGetNext(Lexer);
        }
        
        case '\0':
        {
            Token.Type = WTokenType_EOF;
            Token.Text = Lexer->At;
            Token.Length = 1;
            return Token;
        }
        
        case '\"':
        {
            Token.Text = Lexer->At + 1;
            if (!MoveToEndOfString(Lexer))
            {
                Token.Type = WTokenType_IncompleteString;
                Token.Length = (size_t)(Lexer->At - Token.Text);
            }
            else
            {
                Token.Type = WTokenType_String;
                Token.Length = (size_t)(Lexer->At - Token.Text);
                Advance(Lexer);
            }
            
            return Token;
        }
        
        case ',': Token.Type = WTokenType_Comma; break;
        case '.': Token.Type = WTokenType_Dot; break;
        case '*': Token.Type = WTokenType_Asterisk; break;
        case '/': Token.Type = WTokenType_ForwardSlash; break;
        case '(': Token.Type = WTokenType_LeftParen; break;
        case ')': Token.Type = WTokenType_RightParen; break;
        case '[': Token.Type = WTokenType_LeftSquare; break;
        case ']': Token.Type = WTokenType_RightSquare; break;
        case ';': Token.Type = WTokenType_SemiColon; break;
        case '+':
        {
            if (Lexer->At[1] != '+')
            {
                Token.Type = WTokenType_Plus;
            }
            else
            {
                Token.Type = WTokenType_PlusPlus;
                Token.Text = Lexer->At;
                Token.Length = 2;
                Advance(Lexer);
                Advance(Lexer);
                return Token;
            }
            
            break;
        }
        case '-':
        {
            if (Lexer->At[1] != '-')
            {
                Token.Type = WTokenType_Minus;
            }
            else
            {
                Token.Type = WTokenType_MinusMinus;
                Token.Text = Lexer->At;
                Token.Length = 2;
                Advance(Lexer);
                Advance(Lexer);
                return Token;
            }
            
            break;
        }
        case '<':
        {
            if (Lexer->At[1] != '=')
            {
                Token.Type = WTokenType_LessThan;
            }
            else
            {
                Token.Type = WTokenType_LessThanOrEquals;
                Token.Text = Lexer->At;
                Token.Length = 2;
                Advance(Lexer);
                Advance(Lexer);
                return Token;
            }
            
            break;
        }
        case '>':
        {
            if (Lexer->At[1] != '=')
            {
                Token.Type = WTokenType_GreaterThan;
            }
            else
            {
                Token.Type = WTokenType_GreaterThanOrEquals;
                Token.Text = Lexer->At;
                Token.Length = 2;
                Advance(Lexer);
                Advance(Lexer);
                return Token;
            }
            
            break;
        }
        case '|':
        {
            if (Lexer->At[1] == '|')
            {
                Token.Type = WTokenType_BooleanOr;
                Token.Text = Lexer->At;
                Token.Length = 2;
                Advance(Lexer);
                Advance(Lexer);
                return Token;
            }
            
            break;
        }
        case '&':
        {
            if (Lexer->At[1] == '&')
            {
                Token.Type = WTokenType_BooleanAnd;
                Token.Text = Lexer->At;
                Token.Length = 2;
                Advance(Lexer);
                Advance(Lexer);
                return Token;
            }
            
            break;
        }
        case '=':
        {
            if (Lexer->At[1] != '=')
            {
                Token.Type = WTokenType_Assignment;
            }
            else
            {
                Token.Type = WTokenType_Equals;
                Token.Text = Lexer->At;
                Token.Length = 2;
                Advance(Lexer);
                Advance(Lexer);
                return Token;
            }
            break;
        }
        
        case '!':
        {
            if (Lexer->At[1] != '=')
            {
                Token.Type = WTokenType_Exclamation;
            }
            else
            {
                Token.Type = WTokenType_NotEquals;
                Token.Text = Lexer->At;
                Token.Length = 2;
                Advance(Lexer);
                Advance(Lexer);
                return Token;
            }
            break;
        }
    }
    
    if (Token.Type != WTokenType_Unknown)
    {
        Token.Text = Lexer->At;
        Token.Length = 1;
        Advance(Lexer);
        return Token;
    }
    
    Token.Text = Lexer->At;
    char *Begin = Lexer->At;
    bool IsNumber = true;
    
    for (;;)
    {
        switch (*Lexer->At)
        {
            case '\n':
            case ',':
            case '.':
            case '+':
            case '-':
            case '*':
            case '/':
            case '(':
            case ')':
            case '[':
            case ']':
            case ';':
            case '\0':
            case '\r':
            case '\t':
            case '!':
            case '=':
            case '<':
            case '>':
            case ' ':
            case '$':
            {
                Token.Length = (size_t)(Lexer->At - Begin);
                
                if (IsNumber)
                {
                    Token.Type = WTokenType_Number;
                }
                else if (!SetKeyword(&Token))
                {
                    Token.Type = WTokenType_Identifier;
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
            }
        }
    }
}

static
write_token TextModeGetNext(write_lexer *Lexer)
{
    Lexer->Line = Lexer->NextLine;
    Lexer->Column = Lexer->NextColumn;
    
    char *Begin = Lexer->At;
    write_token Token;
    Token.Text = Lexer->At;
    Token.Type = WTokenType_Text;
    
    if (*Lexer->At == '\0')
    {
        Token.Length = 1;
        Token.Type = WTokenType_EOF;
        return Token;
    }
    else if (*Lexer->At == '\n')
    {
        Token.Length = 1;
        Token.Type = WTokenType_TextNewLine;
        Lexer->NextColumn = 0;
        ++Lexer->NextLine;
        Advance(Lexer);
        return Token;
    }
    
    for(;;)
    {
        switch (*Lexer->At)
        {
            case '$':
            {
                Token.Length = (size_t)(Lexer->At - Begin);
                Advance(Lexer);
                Lexer->Mode = Mode_Expression;
                
                if (Token.Length == 0)
                {
                    Lexer->Flags |= WLexerFlag_SilentlyCrossedExpressionBounds;
                    
                    // special case if the file starts with a '$' or
                    // if there are 2 back to back '$'
                    return ExpressionModeGetNext(Lexer);
                }
                else
                {
                    Lexer->Flags |= WLexerFlag_WillCrossExpressionBounds;
                }
                
                return Token;
            }
            
            case '\n':
            case '\0':
            {
                Token.Length = (size_t)(Lexer->At - Begin);
                return Token;
            }
            
            default:
            {
                Advance(Lexer);
                break;
            }
        }
    }
}

static inline
write_token NextToken(write_lexer *Lexer)
{
    Lexer->Flags = 0;
    if (Lexer->Mode == Mode_Text)
    {
        return TextModeGetNext(Lexer);
    }
    else // Mode_Expression
    {
        return ExpressionModeGetNext(Lexer);
    }
}

void NextTokenInfo(write_lexer *Lexer, wtoken_info *Result)
{
    Result->Flags = 0;
    if (Lexer->Flags & WLexerFlag_WillCrossExpressionBounds)
    {
        Result->Flags |= WTokenFlag_FirstAfterModeSwitch;
    }
    
    Result->Token = NextToken(Lexer);
    Result->Filename = Lexer->Filename;
    Result->Line = Lexer->Line;
    Result->Column = Lexer->Column;
    
    if (Lexer->Flags & WLexerFlag_SilentlyCrossedExpressionBounds)
    {
        Result->Flags |= WTokenFlag_FirstAfterModeSwitch;
    }
}
