#pragma once

#include "numeric_types.h"

enum write_token_type
{
    WTokenType_Unknown,
    WTokenType_EOF,
    WTokenType_Text,
    WTokenType_TextNewLine,
    
    WTokenType_Comma,
    WTokenType_Dot,
    WTokenType_Plus,
    WTokenType_Minus,
    WTokenType_Asterisk,
    WTokenType_ForwardSlash,
    WTokenType_Exclamation,
    WTokenType_LeftParen,
    WTokenType_RightParen,
    WTokenType_LeftSquare,
    WTokenType_RightSquare,
    WTokenType_Equals,
    WTokenType_NotEquals,
    WTokenType_GreaterThan,
    WTokenType_LessThan,
    WTokenType_GreaterThanOrEquals,
    WTokenType_LessThanOrEquals,
    WTokenType_BooleanOr,
    WTokenType_BooleanAnd,
    WTokenType_PlusPlus,
    WTokenType_MinusMinus,
    
    WTokenType_Assignment,
    WTokenType_SemiColon,
    
    WTokenType_Identifier,
    WTokenType_Number,
    WTokenType_String,
    
    WTokenType_If,
    WTokenType_End,
    WTokenType_ForEach,
    WTokenType_For,
    WTokenType_In,
    WTokenType_IgnoreNewLine,
    WTokenType_Define,
    WTokenType_Definitions,
    WTokenType_BeginTab,
    WTokenType_HasAttribute,
    WTokenType_Breakpoint,
    
    // Error types
    WTokenType_IncompleteString
};

struct write_token
{
    write_token_type Type;
    char *Text;
    size_t Length;
};

enum write_lexer_mode
{
    Mode_Text,
    Mode_Expression,
};

enum wlexer_flags
{
    // NOTE(Brian): We need this flag and WLexerFlag_SilentlyCrossedExpressionBounds
    // because the parser doesn't get a token for '$' so it won't know when we
    // cross expression boundaries. In cases where we aren't immediately evaluating an
    // expression and that expression is followed by a text section
    // (ex. we don't evaluate the increment expression of a for loop until after
    // we evaluate the body) We need a reliable way to skip over that expression.
    WLexerFlag_WillCrossExpressionBounds = 1,
    
    // NOTE(Brian): This will happen when there is an empty $$ block.
    WLexerFlag_SilentlyCrossedExpressionBounds = 2,
};

struct write_lexer
{
    char *At;
    char *Begin;
    char *Filename;
    write_lexer_mode Mode;
    uint32 Flags;
    
    int NextLine;
    int NextColumn;
    
    int Line;
    int Column;
    
    int32 AutoClearNewLineTop;
    uint64 AutoClearNewLineStack; // Allows for 64 levels of nesting.
};

enum wtoken_flags : uint32
{
    WTokenFlag_FirstAfterModeSwitch = 1,
};

struct wtoken_info
{
    char *Filename;
    uint32 Flags;
    int Line;
    int Column;
    write_token Token;
};

wtoken_info NextTokenInfo(write_lexer *Lexer);
bool CreateLexer(write_lexer *Lexer, const char *Filename);
void FreeLexer(write_lexer *Lexer);
