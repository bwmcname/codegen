#pragma once

#include "codegen_lex_inspect.h"
#include "codegen_inspect_data.h"
#include "token_stack.h"
#include <vector>

struct type;
struct type_args
{
    std::vector<type> Args;
};

struct type
{
    itoken_info TypeName;
    type_args Args;
    
    bool IsPointer;
    bool IsReference;
    
    type *InnerType;
};

struct typed_argument_declaration
{
    itoken_info Name;
    type Type;
};

struct typed_argument_list_declaration
{
    std::vector<typed_argument_declaration> Arguments;
};

struct field
{
    type Type;
    itoken_info Name;
    attribute_list *Attributes;
    
    bool HasInitializer;
    inspect_ctext InitializerText;
    
    bool IsMethod;
    typed_argument_list_declaration Arguments;
};

struct defined_struct
{
    itoken_info Identifier;
    std::vector<field> Fields;
};

struct declared_type
{
    itoken_info TypeName;
    itoken_info DescriptorName;
};

struct import_file
{
    itoken_info Filename;
};

struct lex_state
{
    itoken_info At;
    inspect_lexer *Lexer;
};

struct lexer_stack
{
    static const int Max = 10;
    lex_state Lexers[Max];
    int Top;
};

struct argument_list_declaration
{
    std::vector<itoken_info> Names;
};

struct attribute_declaration
{
    itoken_info Name;
    argument_list_declaration ArgumentList;
};

struct attribute_alias
{
    itoken_info Alias;
    attribute_instance Value;
};

struct inspect_parser
{
    token_stack<itoken_info> Stack;
    
    std::vector<inspect_lexer *> LexerStorage;
    lexer_stack LexerStack;
    
    inspect_lexer *Lexer;
    itoken_info At;
    
    inspect_data_item StructList;
    inspect_data_item TypeInfoList;
    std::vector<attribute_declaration> AttributeInformation;
    
    std::vector<attribute_list *> UnresolvedAttributeLists;
    std::vector<attribute_alias> AttributeAliases;
};

bool ParseInspect(inspect_parser *Parser, inspect_data *Data);

static inline
void CreateInspectData(inspect_data *Data)
{
    Data->GlobalScope = NewDictItem();
}

static inline
void FreeInspectData(inspect_data *Data)
{
    FreeDataItem(&Data->GlobalScope);
}

bool CreateParser(const char *Filename, inspect_parser *Parser);
void FreeParser(inspect_parser *Parser);