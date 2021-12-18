#pragma once
#include <stdio.h>
#include <assert.h>
#include "codegen_lex_write.h"
#include "codegen_inspect_data.h"
#include "token_stack.h"

struct write_parser;

struct data_item_stack
{
    // NOTE(Brian): This will stop working if we recursively collect items.
    std::vector<std::vector<inspect_data_item>> Stack;
    
    inline void PushFrame()
    {
        Stack.emplace_back();
    }
    
    inline void PushItem(inspect_data_item Item)
    {
        Stack.back().push_back(Item);
    }
    
    inline void PopFrame()
    {
        for(inspect_data_item &Item : Stack.back())
        {
            FreeDataItem(&Item);
        }
        
        Stack.pop_back();
    }
};

enum write_parser_flags : uint32
{
    WP_IllegalExpressionReported = 0x1,
    WP_ShouldAdjustTabs = 0x2,
    WP_UseSpacesInsteadOfTabs = 0x4,
};

#define DEFAULT_OUTPUT_SIZE 1024

struct write_parser
{
    write_lexer Lexer;
    token_stack<wtoken_info> Stack; // TODO(Brian): This isn't really a stack... we should rename this.
    FILE *Output;
    
    // NOTE(Brian): Stack of temporary memory, not necassarily a variable stack.
    // the "Scope" dict passed along various parse functions acts more as a variable
    // stack.
    data_item_stack ItemStack;
    
    uint32 Flags;
    
    uint64 AutoClearNewLineStack; // Should support nesting up to 64 levels
    uint64 AutoClearNewLineTop;
    
    int32 TabsToRemove;
    int32 TabsToAdd;
    int32 TabsAdded;
    int32 TabsRemoved;
    int32 TabSize;
    int32 QueuedTabs;
    
    size_t OutputBufferSize;
    char *OutputBuffer;
};

struct stack_frame
{
    stack_frame(write_parser *Parser)
        : Stack(&Parser->ItemStack)
    {
        Stack->PushFrame();
    }
    
    stack_frame(const stack_frame &) = delete;
    
    ~stack_frame()
    {
        Stack->PopFrame();
    }
    
    inline void TryReleaseItem(inspect_data_item *Item)
    {
        std::vector<inspect_data_item> &Frame = Stack->Stack.back();
        for (auto It = Frame.begin();
             It != Frame.end();
             It++)
        {
            if (It->UID == Item->UID)
            {
                Frame.erase(It);
                return;
            }
        }
    }
    
    private:
    data_item_stack *Stack;
};

inline
inspect_data_item NewStringItem(wtoken_info *Token)
{
    inspect_data_item Item;
    Item.Type = Type_String;
    Item.String = (char *)malloc(Token->Token.Length + 1);
    memcpy(Item.String, Token->Token.Text, Token->Token.Length);
    Item.String[Token->Token.Length] = '\0';
    return Item;
}

bool EvaluateTemplate(write_parser *Parser, inspect_dict *Scope);
void FreeParser(write_parser *Parser);
bool CreateParser(write_parser *Parser, const char *Filename, const char *OutputFilename);