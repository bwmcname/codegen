#pragma once

#include <stdio.h>
#include <assert.h>
#include "codegen_lex_write.h"
#include "codegen_parse_base.h"
#include "codegen_inspect_data.h"
#include "codegen_parse_inspect.h"
#include "token_stack.h"
#include "compiler_utils.h"

// Some Guidelines...
// TryParse* or TryEvaluate* should leave the current token to be the next unparsed token.
// tryParse* or TryEvaluate* should expect that the current token is the first unparsed token.

static
bool Evaluate(write_parser *Parser, inspect_dict *Scope, write_token_type Until);
static inline
bool ShouldIgnoreNewLine(write_parser *Parser);
static inline
void PushScopeLevel(write_parser *Parser, bool IgnoreNewLines, bool IncreaseTabLevel);
static inline
void PopScopeLevel(write_parser *Parser, bool PopTabLevel);
static inline
void SetLineBeginTabState(write_parser *Parser);
static inline
void SetIgnoreLineBeginTabState(write_parser *Parser);
static inline
void RestoreTabState(write_parser *Parser, tab_state *State);
static inline
tab_state SaveTabState(write_parser *Parser);

struct parser_state
{
    uint64 AutoClearNewLineStack;
    uint64 AutoClearNewLineTop;
    
    tab_state TabState;
};

static inline
parser_state SaveParserInfo(write_parser *Parser);
static inline
void RestoreParserInfo(write_parser *Parser, parser_state *State);

bool CreateParser(write_parser *Parser, const char *Filename, const char *OutputFilename)
{
    if (!CreateLexer(&Parser->Lexer, Filename))
    {
        return false;
    }
    
    CreateTokenStack(&Parser->Stack);
    
    Parser->Output = fopen(OutputFilename, "w");
    if (!Parser->Output)
    {
        return false;
    }
    
    Parser->AutoClearNewLineStack = 0;
    Parser->AutoClearNewLineTop = 0;
    Parser->TabsToRemove = 0;
    Parser->TabsToAdd = 0;
    Parser->TabsAdded = 0;
    Parser->TabsRemoved = 0;
    Parser->TabSize = 4; // Should be a command option?
    Parser->QueuedTabs = 0;
    
    Parser->Flags = 0;
    Parser->Flags |= (WP_ShouldAdjustTabs | WP_UseSpacesInsteadOfTabs); // should be an option?
    
    Parser->OutputBufferSize = DEFAULT_OUTPUT_SIZE;
    Parser->OutputBuffer = (char *)malloc(Parser->OutputBufferSize);
    
    return true;
}


void FreeParser(write_parser *Parser)
{
    FreeLexer(&Parser->Lexer);
    FreeTokenStack(&Parser->Stack);
    
    free(Parser->OutputBuffer);
}

static inline
wtoken_info Current(write_parser *Parser)
{
    return Parser->Stack.Tokens[Parser->Stack.Top];
}

static
bool PushToken(write_parser *Parser, wtoken_info *Result)
{
    Parser->Stack.Top++;
    
    if (Parser->Stack.Top < Parser->Stack.Populated)
    {
        // Have already added this token.
        *Result = Parser->Stack.Tokens[Parser->Stack.Top];
        return true;
    }
    
    if (Parser->Stack.Top == Parser->Stack.Capacity)
    {
        Resize(&Parser->Stack);
    }
    
    NextTokenInfo(&Parser->Lexer, Result);
    if (Result->Token.Type == WTokenType_IncompleteString)
    {
        PrintLocation(Parser->Lexer.Line,
                      Parser->Lexer.Column,
                      Parser->Lexer.Filename);
        printf("Incomplete string\n");
        return false;
    }
    
    Parser->Stack.Tokens[Parser->Stack.Top] = *Result;
    ++Parser->Stack.Populated;
    return true;
}

static inline
bool PushToken(write_parser *Parser)
{
    wtoken_info Current;
    return PushToken(Parser, &Current);
}

static
void PopTokens(write_parser *Parser, int Count)
{
    Parser->Stack.Top -= Count;
}

static
void Jump(write_parser *Parser, int Top)
{
    Parser->Stack.Top = Top;
}

static constexpr char *ListSizeKeyword = "Size";

static inline
bool GetListVariable(wtoken_info *Identifier, inspect_list *List, inspect_data_item *Result)
{
    if (ConstexprStrlen(ListSizeKeyword) == Identifier->Token.Length &&
        strncmp(ListSizeKeyword, Identifier->Token.Text, Identifier->Token.Length) == 0)
    {
        *Result = NewIntItem((int)List->size());
        return true;
    }
    
    return false;
}

static inline
bool GetVariable(wtoken_info *Identifier, inspect_data_item *Scope, inspect_data_item *Result)
{
    if (Scope->Type == Type_Dict)
    {
        return Lookup(Scope->Dict, &Identifier->Token, Result);
    }
    else if (Scope->Type == Type_List)
    {
        return GetListVariable(Identifier, Scope->List, Result);
    }
    else
    {
        // NOTE(Brian): We should have handled this before we got here.
        assert(false);
    }
    
    return true;
}

static
void HandleUnexpectedEnd(wtoken_info *Info)
{
    PrintLocation(Info->Line, Info->Column, Info->Filename);
    printf("Unexpected end of file.\n");
}

static
bool TryEvaluateSubExpression(write_parser *Parser,
                              inspect_dict *Scope,
                              inspect_data_item *Result,
                              stack_frame *Frame);

static
bool TryEvaluateReference(write_parser *Parser,
                          inspect_data_item *PathScope,
                          inspect_dict *Scope,
                          inspect_data_item *Result);

static void HandleFailedVariablePathResolution(wtoken_info AfterDot)
{
    PrintLocation(AfterDot.Line, AfterDot.Column, AfterDot.Filename);
    if (AfterDot.Token.Type == WTokenType_Identifier)
    {
        printf("Invalid identifier \"%.*s\"\n",
               (int)AfterDot.Token.Length,
               AfterDot.Token.Text);
    }
    else
    {
        printf("Expected identifier\n");
    }
}

static inline
bool IsScopeStarter(wtoken_info *Token)
{
    return Token->Token.Type == WTokenType_ForEach ||
        Token->Token.Type == WTokenType_For ||
        Token->Token.Type == WTokenType_If ||
        Token->Token.Type == WTokenType_Define ||
        Token->Token.Type == WTokenType_Definitions ||
        Token->Token.Type == WTokenType_BeginTab;
}

static inline
bool SkipPastMatchingEnd(write_parser *Parser)
{
    wtoken_info Begin = Current(Parser);
    wtoken_info Next = Begin;
    
    int ScopesStarted = 0;
    for (;;)
    {
        if (Next.Token.Type == WTokenType_EOF)
        {
            PrintLocation(Begin.Line, Begin.Column, Begin.Filename);
            printf("EOF reached before scope closed. Are you missing an end?\n");
            return false;
        }
        
        if (IsScopeStarter(&Next))
        {
            ++ScopesStarted;
        }
        
        if (Next.Token.Type == WTokenType_End)
        {
            if (ScopesStarted == 0)
            {
                return PushToken(Parser);
            }
            
            --ScopesStarted;
        }
        
        if (!PushToken(Parser, &Next))
        {
            return false;
        }
    }
}

#if 0
static
bool PeekNext(write_parser *Parser, wtoken_info *Token)
{
    if (!PushToken(Parser, Token))
    {
        return false;
    }
    
    PopTokens(Parser, 1);
    return true;
}
#endif

static
bool TryEvaluateDefine(write_parser *Parser,
                       inspect_dict *Scope)
{
    wtoken_info CurrentToken = Current(Parser);
    if (CurrentToken.Token.Type != WTokenType_Define)
    {
        return false;
    }
    
    wtoken_info Name;
    if (!PushToken(Parser, &Name))
    {
        return false;
    }
    
    if (Name.Token.Type != WTokenType_Identifier)
    {
        PrintLocation(Name.Line, Name.Column, Name.Filename);
        printf("Invalid identifier \"%.*s\"\n", (int)Name.Token.Length, Name.Token.Text);
        return false;
    }
    
    if (!PushToken(Parser, &CurrentToken))
    {
        return false;
    }
    
    if (CurrentToken.Token.Type != WTokenType_LeftParen)
    {
        return false;
    }
    
    if (!PushToken(Parser, &CurrentToken))
    {
        return false;
    }
    
    inspect_data_item ProcedureItem = NewProcedureItem();
    inspect_procedure &Procedure = *ProcedureItem.Procedure;
    
    if (CurrentToken.Token.Type != WTokenType_RightParen)
    {
        for (;;)
        {
            if (CurrentToken.Token.Type != WTokenType_Identifier)
            {
                FreeDataItem(&ProcedureItem);
                PrintLocation(CurrentToken.Line, CurrentToken.Column, CurrentToken.Filename);
                printf("Expected identifier, got \"%.*s\"\n",
                       (int)CurrentToken.Token.Length,
                       CurrentToken.Token.Text);
                return false;
            }
            
            Procedure.Args.push_back(CurrentToken);
            
            if (!PushToken(Parser, &CurrentToken))
            {
                FreeDataItem(&ProcedureItem);
                return false;
            }
            
            if (CurrentToken.Token.Type == WTokenType_RightParen)
            {
                if (!PushToken(Parser, &CurrentToken))
                {
                    return false;
                }
                
                break;
            }
            
            if (CurrentToken.Token.Type != WTokenType_Comma)
            {
                FreeDataItem(&ProcedureItem);
                PrintLocation(CurrentToken.Line, CurrentToken.Column, CurrentToken.Filename);
                printf("Expected \",\", got \"%.*s\"\n",
                       (int)CurrentToken.Token.Length,
                       CurrentToken.Token.Text);
                return false;
            }
            
            if (!PushToken(Parser, &CurrentToken))
            {
                return false;
            }
        }
    }
    else
    {
        if (!PushToken(Parser, &CurrentToken))
        {
            return false;
        }
    }
    
    Procedure.BodyLocation = Parser->Stack.Top;
    Procedure.ParentScope = Scope;
    Procedure.TabState.TabsToAdd = Parser->TabsToAdd;
    Procedure.TabState.TabsToRemove = Parser->TabsToRemove + 1; // This function starts another tab scope.
    if (!SkipPastMatchingEnd(Parser))
    {
        FreeDataItem(&ProcedureItem);
        return false;
    }
    
    Insert(Scope, &Name.Token, &ProcedureItem);
    return true;
}

static
bool TryEvaluateParanthesis(write_parser *Parser,
                            inspect_dict *Scope,
                            inspect_data_item *Result)
{
    wtoken_info CurrentToken = Current(Parser);
    if (CurrentToken.Token.Type != WTokenType_LeftParen)
    {
        return false;
    }
    
    if (!PushToken(Parser))
    {
        return false;
    }
    
    stack_frame NewFrame(Parser);
    if (!TryEvaluateSubExpression(Parser, Scope, Result, &NewFrame))
    {
        return false;
    }
    
    wtoken_info RightParen = Current(Parser);
    
    if (RightParen.Token.Type != WTokenType_RightParen)
    {
        PrintLocation(CurrentToken.Line, CurrentToken.Column, CurrentToken.Filename);
        printf("Unmatched parenthesis\n");
        return false;
    }
    
    if (!PushToken(Parser))
    {
        return false;
    }
    
    return true;
}

#if 0
static inline
bool CompareCStringAndToken(const char *CString, write_token *Token)
{
    size_t I;
    for (I = 0; I < Token->Length; ++I)
    {
        if (CString[I] == '\0')
        {
            return false;
        }
        
        if (CString[I] != Token->Text[I])
        {
            return false;
        }
    }
    
    return CString[I] == '\0';
}
#endif

static
bool TryEvaluateIndexer(write_parser *Parser,
                        inspect_data_item *ToIndex,
                        inspect_dict *Scope,
                        inspect_data_item *Result)
{
    wtoken_info CurrentToken = Current(Parser);
    if (CurrentToken.Token.Type != WTokenType_LeftSquare)
    {
        return false;
    }
    
    if (!PushToken(Parser, &CurrentToken))
    {
        return false;
    }
    
    stack_frame NewFrame(Parser);
    inspect_data_item Indice;
    if (!TryEvaluateSubExpression(Parser, Scope, &Indice, &NewFrame))
    {
        return false;
    }
    
    if (Indice.Type != Type_Int &&
        Indice.Type != Type_String)
    {
        PrintLocation(CurrentToken.Line, CurrentToken.Column, CurrentToken.Filename);
        printf ("Invalid index. Expression must evaluate to an integer or a string.\n");
        return false;
    }
    
    inspect_data_item Indexed;
    if (Indice.Type == Type_Int)
    {
        Indexed = ToIndex->List->at((size_t)Indice.Int);
    }
    else // Indice.Type == Type_String
    {
        // Look up the attribute value
        if (!Lookup(&ToIndex->Attributes->AttributeData, Indice.String, &Indexed))
        {
            PrintLocation(CurrentToken.Line,
                          CurrentToken.Column,
                          CurrentToken.Filename);
            printf ("Unable to find attribute \"%s\"\n", Indexed.String);
            return false;
        }
    }
    
    CurrentToken = Current(Parser);
    
    if (CurrentToken.Token.Type != WTokenType_RightSquare)
    {
        PrintLocation(CurrentToken.Line, CurrentToken.Column, CurrentToken.Filename);
        printf("Expected \"]\"\n");
        return false;
    }
    
    if (!PushToken(Parser, &CurrentToken))
    {
        return false;
    }
    
    if (CurrentToken.Token.Type == WTokenType_Dot)
    {
        if (!PushToken(Parser))
        {
            return false;
        }
        
        if (!TryEvaluateReference(Parser, &Indexed, Scope, Result))
        {
            HandleFailedVariablePathResolution(CurrentToken);
        }
        
        return true;
    }
    else if (TryEvaluateIndexer(Parser, &Indexed, Scope, Result))
    {
        return true;
    }
    else
    {
        *Result = Indexed;
        return true;
    }
}

static
bool TryEvaluateReference(write_parser *Parser,
                          inspect_data_item *PathScope,
                          inspect_dict *Scope,
                          inspect_data_item *Result)
{
    wtoken_info Identifier = Current(Parser);
    
    if (Identifier.Token.Type != WTokenType_Identifier)
    {
        return false;
    }
    
    inspect_data_item IdentifierItem;
    if (!GetVariable(&Identifier, PathScope, &IdentifierItem))
    {
        return false;
    }
    
    wtoken_info Next;
    if (!PushToken(Parser, &Next))
    {
        return false;
    }
    
    if (TryEvaluateIndexer(Parser, &IdentifierItem, Scope, Result))
    {
        return true;
    }
    else if (Next.Token.Type == WTokenType_Dot)
    {
        wtoken_info AfterDot;
        if (!PushToken(Parser, &AfterDot))
        {
            return false;
        }
        
        if (!TryEvaluateReference(Parser, &IdentifierItem, Scope, Result))
        {
            HandleFailedVariablePathResolution(AfterDot);
            
            return false;
        }
        
        return true;
    }
    else
    {
        *Result = IdentifierItem;
        
        return true;
    }
}

static
std::string FindLValueName(inspect_data_item *Item)
{
    // NOTE(Brian): We should have confirmed that this item is
    // and L-Value before we called this function.
    assert(Item->Owner != nullptr);
    
    for (auto It = Item->Owner->Lookup.begin(); It != Item->Owner->Lookup.end(); It++)
    {
        if (It->second.UID == Item->UID)
        {
            return It->first;
        }
    }
    
    // NOTE(Brian): We shouldn't be calling this function unless we know the value
    // in the dictionary.
    assert(false);
    return {};
}

static
void PrintInvalidOperation(wtoken_info *ExpressionToken,
                           inspect_data_item *ExpressionItem,
                           const char *Operator)
{
    PrintLocation(ExpressionToken->Line, ExpressionToken->Column, ExpressionToken->Filename);
    printf("Operator \"%s\" not valid on type \"%s\"\n",
           Operator,
           InspectItemTypeToString(ExpressionItem->Type));
}

static
void PrintInvalidCast(inspect_item_type Type,
                      inspect_data_item *Item,
                      wtoken_info *ItemToken)
{
    PrintLocation(ItemToken->Line, ItemToken->Column, ItemToken->Filename);
    printf("Invalid cast from type \"%s\" to \"%s\"\n",
           InspectItemTypeToString(Type),
           InspectItemTypeToString(Item->Type));
}

inline static
int NumberTokenToInt(wtoken_info *Number)
{
    int final = 0;
    for (size_t i = 0; i < Number->Token.Length; ++i)
    {
        final *= 10;
        final += (Number->Token.Text[i] - 48);
    }
    
    return final;
}

static
bool TryParseIntegerLiteral(write_parser *Parser,
                            inspect_data_item *Result)
{
    wtoken_info CurrentToken = Current(Parser);
    if (CurrentToken.Token.Type != WTokenType_Number)
    {
        return false;
    }
    
    *Result = NewIntItem(NumberTokenToInt(&CurrentToken));
    Parser->ItemStack.PushItem(*Result);
    return PushToken(Parser);
}

static
bool TryParseStringLiteral(write_parser *Parser,
                           inspect_data_item *Result)
{
    wtoken_info CurrentToken = Current(Parser);
    if (CurrentToken.Token.Type != WTokenType_String)
    {
        return false;
    }
    
    *Result = NewStringItem(&CurrentToken);
    Parser->ItemStack.PushItem(*Result);
    return PushToken(Parser);
}

static
bool TryEvaluateProcedureCall(write_parser *Parser,
                              inspect_dict *Scope,
                              inspect_data_item *Result);

static
bool TryEvaluateHasAttribute(write_parser *Parser,
                             inspect_dict *Scope,
                             inspect_data_item *Result);

static
bool TryEvaluateSimple(write_parser *Parser,
                       inspect_dict *Scope,
                       inspect_data_item *Result)
{
    inspect_data_item ScopeReference = CreateReference(Scope);
    if (TryEvaluateProcedureCall(Parser, Scope, Result) ||
        TryEvaluateParanthesis(Parser, Scope, Result) ||
        TryEvaluateReference(Parser, &ScopeReference, Scope, Result) ||
        TryParseIntegerLiteral(Parser, Result) ||
        TryParseStringLiteral(Parser, Result) ||
        TryEvaluateHasAttribute(Parser, Scope, Result))
    {
        FreeDataItem(&ScopeReference);
        return true;
    }
    
    FreeDataItem(&ScopeReference);
    return false;
}

static
bool TryEvaluatePreIncrement(write_parser *Parser,
                             inspect_dict *Scope,
                             inspect_data_item *Result)
{
    wtoken_info CurrentToken = Current(Parser);
    if (CurrentToken.Token.Type != WTokenType_PlusPlus)
    {
        return TryEvaluateSimple(Parser, Scope, Result);
    }
    
    if (!PushToken(Parser, &CurrentToken))
    {
        return false;
    }
    
    stack_frame NewFrame(Parser);
    inspect_data_item ToIncrement;
    if (!TryEvaluateSubExpression(Parser, Scope, &ToIncrement, &NewFrame))
    {
        return false;
    }
    
    if (ToIncrement.Owner == nullptr)
    {
        PrintLocation(CurrentToken.Line, CurrentToken.Column, CurrentToken.Filename);
        printf ("Pre-increment must be followed by an L-Value\n");
        return false;
    }
    
    inspect_data_operation_interface *Interface = GetInterface(ToIncrement.Type);
    if (!Interface->CanExecuteOperation(Increment_Op))
    {
        PrintInvalidOperation(&CurrentToken, &ToIncrement, "++");
        return false;
    }
    
    *Result = Interface->Increment(&ToIncrement);
    
    std::string AssignmentName = FindLValueName(&ToIncrement);
    FreeIfExists(ToIncrement.Owner, AssignmentName.c_str());
    Insert(ToIncrement.Owner, AssignmentName.c_str(), Result);
    NewFrame.TryReleaseItem(Result); // NOTE(Brian): The item is now "owned" by the scope.
    
    return true;
}

static
bool TryEvaluatePreDecrement(write_parser *Parser,
                             inspect_dict *Scope,
                             inspect_data_item *Result)
{
    wtoken_info CurrentToken = Current(Parser);
    if (CurrentToken.Token.Type != WTokenType_MinusMinus)
    {
        return TryEvaluatePreIncrement(Parser, Scope, Result);
    }
    
    if (!PushToken(Parser, &CurrentToken))
    {
        return false;
    }
    
    stack_frame NewFrame(Parser);
    inspect_data_item ToIncrement;
    if (!TryEvaluateSubExpression(Parser, Scope, &ToIncrement, &NewFrame))
    {
        return false;
    }
    
    if (ToIncrement.Owner == nullptr)
    {
        PrintLocation(CurrentToken.Line, CurrentToken.Column, CurrentToken.Filename);
        printf ("Pre-decrement must be followed by an L-Value\n");
        return false;
    }
    
    inspect_data_operation_interface *Interface = GetInterface(ToIncrement.Type);
    if (!Interface->CanExecuteOperation(Increment_Op))
    {
        PrintInvalidOperation(&CurrentToken, &ToIncrement, "--");
        return false;
    }
    
    *Result = Interface->Decrement(&ToIncrement);
    
    std::string AssignmentName = FindLValueName(&ToIncrement);
    FreeIfExists(ToIncrement.Owner, AssignmentName.c_str());
    Insert(ToIncrement.Owner, AssignmentName.c_str(), Result);
    NewFrame.TryReleaseItem(Result); // NOTE(Brian): The item is now "owned" by the scope.
    
    return true;
}

static
bool TryEvaluatePostIncrement(write_parser *Parser,
                              inspect_dict *Scope,
                              inspect_data_item *Result)
{
    inspect_data_item ToIncrement;
    if (!TryEvaluatePreDecrement(Parser, Scope, &ToIncrement))
    {
        return false;
    }
    
    wtoken_info CurrentToken = Current(Parser);
    if (CurrentToken.Token.Type != WTokenType_MinusMinus)
    {
        *Result = ToIncrement;
        return true;
    }
    
    if (ToIncrement.Owner == nullptr)
    {
        PrintLocation(CurrentToken.Line, CurrentToken.Column, CurrentToken.Filename);
        printf ("Post-decrement must be preceded by an L-Value\n");
        return false;
    }
    
    inspect_data_operation_interface *Interface = GetInterface(ToIncrement.Type);
    if (!Interface->CanExecuteOperation(Increment_Op))
    {
        PrintInvalidOperation(&CurrentToken, &ToIncrement, "--");
        return false;
    }
    
    *Result = ToIncrement;
    inspect_data_item NewValue = Interface->Decrement(&ToIncrement);
    
    std::string AssignmentName = FindLValueName(&ToIncrement);
    FreeIfExists(ToIncrement.Owner, AssignmentName.c_str());
    Insert(ToIncrement.Owner, AssignmentName.c_str(), &NewValue);
    
    return PushToken(Parser);
}

static
bool TryEvaluatePostDecrement(write_parser *Parser,
                              inspect_dict *Scope,
                              inspect_data_item *Result)
{
    inspect_data_item ToDecrement;
    if (!TryEvaluatePostIncrement(Parser, Scope, &ToDecrement))
    {
        return false;
    }
    
    wtoken_info CurrentToken = Current(Parser);
    if (CurrentToken.Token.Type != WTokenType_MinusMinus)
    {
        *Result = ToDecrement;
        return true;
    }
    
    if (ToDecrement.Owner == nullptr)
    {
        PrintLocation(CurrentToken.Line, CurrentToken.Column, CurrentToken.Filename);
        printf ("Post-decrement must be preceded by an L-Value\n");
        return false;
    }
    
    inspect_data_operation_interface *Interface = GetInterface(ToDecrement.Type);
    if (!Interface->CanExecuteOperation(Decrement_Op))
    {
        PrintInvalidOperation(&CurrentToken, &ToDecrement, "--");
        return false;
    }
    
    *Result = ToDecrement;
    inspect_data_item NewValue = Interface->Decrement(&ToDecrement);
    
    std::string AssignmentName = FindLValueName(&ToDecrement);
    FreeIfExists(ToDecrement.Owner, AssignmentName.c_str());
    Insert(ToDecrement.Owner, AssignmentName.c_str(), &NewValue);
    
    return PushToken(Parser);
}

static
bool TryEvaluateNegative(write_parser *Parser,
                         inspect_dict *Scope,
                         inspect_data_item *Result)
{
    wtoken_info LeftToken = Current(Parser);
    if (LeftToken.Token.Type != WTokenType_Minus)
    {
        return TryEvaluatePostDecrement(Parser, Scope, Result);
    }
    
    wtoken_info Next;
    if (!PushToken(Parser, &Next))
    {
        return false;
    }
    
    if (Next.Token.Type == WTokenType_EOF)
    {
        HandleUnexpectedEnd(&Next);
        return false;
    }
    
    inspect_data_item RightItem;
    if (!TryEvaluateNegative(Parser, Scope, &RightItem))
    {
        return false;
    }
    
    inspect_data_operation_interface *Interface = GetInterface(RightItem.Type);
    if (!Interface->CanExecuteOperation(Negative_Op))
    {
        PrintInvalidOperation(&LeftToken, &RightItem, "-");
        return false;
    }
    
    *Result = Interface->Negate(&RightItem);
    return true;
}

static
bool TryEvaluateNot(write_parser *Parser,
                    inspect_dict *Scope,
                    inspect_data_item *Result)
{
    wtoken_info LeftToken = Current(Parser);
    if (LeftToken.Token.Type != WTokenType_Exclamation)
    {
        return TryEvaluateNegative(Parser, Scope, Result);
    }
    
    wtoken_info Next;
    if (!PushToken(Parser, &Next))
    {
        return false;
    }
    
    if (Next.Token.Type == WTokenType_EOF)
    {
        HandleUnexpectedEnd(&Next);
        return false;
    }
    
    inspect_data_item RightItem;
    if (!TryEvaluateNot(Parser, Scope, &RightItem))
    {
        return false;
    }
    
    inspect_data_operation_interface *Interface = GetInterface(RightItem.Type);
    if (!Interface->CanExecuteOperation(Not_Op))
    {
        PrintInvalidOperation(&LeftToken, &RightItem, "!");
        return false;
    }
    
    *Result = Interface->Not(&RightItem);
    return true;
}

static
bool TryEvaluateMultiplication(write_parser *Parser,
                               inspect_dict *Scope,
                               inspect_data_item *Result)
{
    wtoken_info LeftToken = Current(Parser);
    inspect_data_item Left;
    if (!TryEvaluateNot(Parser, Scope, &Left))
    {
        return false;
    }
    
    wtoken_info Next = Current(Parser);
    if (Next.Token.Type != WTokenType_Asterisk)
    {
        *Result = Left;
        return true;
    }
    
    wtoken_info RightToken;
    if (!PushToken(Parser, &RightToken))
    {
        return false;
    }
    
    inspect_data_item Right;
    if (!TryEvaluateParanthesis(Parser, Scope, &Right) &&
        !TryEvaluateMultiplication(Parser, Scope, &Right))
    {
        return false;
    }
    
    inspect_data_operation_interface *LeftInterface = GetInterface(Left.Type);
    
    if (!LeftInterface->CanExecuteOperation(Multiplication_Op))
    {
        PrintInvalidOperation(&LeftToken, &Left, "*");
        return false;
    }
    
    if (Right.Type == Left.Type)
    {
        *Result = LeftInterface->Multiply(&Left, &Right);
    }
    else
    {
        inspect_data_item RightCasted;
        inspect_data_operation_interface *RightInterface = GetInterface(Right.Type);
        if (!RightInterface->Cast(Left.Type, &Right, &RightCasted))
        {
            PrintInvalidCast(Left.Type, &Right, &RightToken);
            return false;
        }
        
        *Result = LeftInterface->Multiply(&Left, &RightCasted);
    }
    
    return true;
}

static
bool TryEvaluateDivision(write_parser *Parser,
                         inspect_dict *Scope,
                         inspect_data_item *Result)
{
    wtoken_info LeftToken = Current(Parser);
    inspect_data_item Left;
    if (!TryEvaluateMultiplication(Parser, Scope, &Left))
    {
        return false;
    }
    
    wtoken_info Next = Current(Parser);
    if (Next.Token.Type != WTokenType_ForwardSlash)
    {
        *Result = Left;
        return true;
    }
    
    wtoken_info RightToken;
    if (!PushToken(Parser, &RightToken))
    {
        return false;
    }
    
    inspect_data_item Right;
    if (!TryEvaluateParanthesis(Parser, Scope, &Right) &&
        !TryEvaluateDivision(Parser, Scope, &Right))
    {
        return false;
    }
    
    inspect_data_operation_interface *LeftInterface = GetInterface(Left.Type);
    
    if (!LeftInterface->CanExecuteOperation(Division_Op))
    {
        PrintInvalidOperation(&LeftToken, &Left, "/");
        return false;
    }
    
    if (Right.Type == Left.Type)
    {
        *Result = LeftInterface->Divide(&Left, &Right);
    }
    else
    {
        inspect_data_item RightCasted;
        inspect_data_operation_interface *RightInterface = GetInterface(Right.Type);
        if (!RightInterface->Cast(Left.Type, &Right, &RightCasted))
        {
            PrintInvalidCast(Left.Type, &Right, &RightToken);
            return false;
        }
        
        *Result = LeftInterface->Divide(&Left, &RightCasted);
    }
    
    return true;
}

static
bool TryEvaluateAddition(write_parser *Parser,
                         inspect_dict *Scope,
                         inspect_data_item *Result)
{
    wtoken_info LeftToken = Current(Parser);
    inspect_data_item Left;
    if (!TryEvaluateDivision(Parser, Scope, &Left))
    {
        return false;
    }
    
    wtoken_info Next = Current(Parser);
    if (Next.Token.Type != WTokenType_Plus)
    {
        *Result = Left;
        return true;
    }
    
    wtoken_info RightToken;
    if (!PushToken(Parser, &RightToken))
    {
        return false;
    }
    
    inspect_data_item Right;
    if (!TryEvaluateParanthesis(Parser, Scope, &Right) &&
        !TryEvaluateAddition(Parser, Scope, &Right))
    {
        return false;
    }
    
    inspect_data_operation_interface *LeftInterface = GetInterface(Left.Type);
    
    if (!LeftInterface->CanExecuteOperation(Addition_Op))
    {
        PrintInvalidOperation(&LeftToken, &Left, "+");
        return false;
    }
    
    if (Right.Type == Left.Type)
    {
        *Result = LeftInterface->Add(&Left, &Right);
    }
    else
    {
        inspect_data_item RightCasted;
        inspect_data_operation_interface *RightInterface = GetInterface(Right.Type);
        if (!RightInterface->Cast(Left.Type, &Right, &RightCasted))
        {
            PrintInvalidCast(Left.Type, &Right, &RightToken);
            return false;
        }
        
        *Result = LeftInterface->Add(&Left, &RightCasted);
    }
    
    return true;
}

static
bool TryEvaluateSubtraction(write_parser *Parser,
                            inspect_dict *Scope,
                            inspect_data_item *Result)
{
    wtoken_info LeftToken = Current(Parser);
    inspect_data_item Left;
    if (!TryEvaluateAddition(Parser, Scope, &Left))
    {
        return false;
    }
    
    wtoken_info Next = Current(Parser);
    if (Next.Token.Type != WTokenType_Minus)
    {
        *Result = Left;
        return true;
    }
    
    wtoken_info RightToken;
    if (!PushToken(Parser, &RightToken))
    {
        return false;
    }
    
    inspect_data_item Right;
    if (!TryEvaluateParanthesis(Parser, Scope, &Right) &&
        !TryEvaluateSubtraction(Parser, Scope, &Right))
    {
        return false;
    }
    
    inspect_data_operation_interface *LeftInterface = GetInterface(Left.Type);
    
    if (!LeftInterface->CanExecuteOperation(Subtraction_Op))
    {
        PrintInvalidOperation(&LeftToken, &Left, "-");
        return false;
    }
    
    if (Right.Type == Left.Type)
    {
        *Result = LeftInterface->Subtract(&Left, &Right);
    }
    else
    {
        inspect_data_item RightCasted;
        inspect_data_operation_interface *RightInterface = GetInterface(Right.Type);
        if (!RightInterface->Cast(Left.Type, &Right, &RightCasted))
        {
            PrintInvalidCast(Left.Type, &Right, &RightToken);
            return false;
        }
        
        *Result = LeftInterface->Subtract(&Left, &RightCasted);
    }
    
    return true;
}

static
bool TryEvaluateGreaterThanOrEquals(write_parser *Parser,
                                    inspect_dict *Scope,
                                    inspect_data_item *Result)
{
    wtoken_info LeftToken = Current(Parser);
    inspect_data_item Left;
    if (!TryEvaluateSubtraction(Parser, Scope, &Left))
    {
        return false;
    }
    
    wtoken_info Next = Current(Parser);
    if (Next.Token.Type != WTokenType_GreaterThanOrEquals)
    {
        *Result = Left;
        return true;
    }
    
    wtoken_info RightToken;
    if (!PushToken(Parser, &RightToken))
    {
        return false;
    }
    
    inspect_data_item Right;
    if (!TryEvaluateParanthesis(Parser, Scope, &Right) &&
        !TryEvaluateGreaterThanOrEquals(Parser, Scope, &Right))
    {
        return false;
    }
    
    inspect_data_operation_interface *LeftInterface = GetInterface(Left.Type);
    
    if (!LeftInterface->CanExecuteOperation(GreaterThan_Op))
    {
        PrintInvalidOperation(&LeftToken, &Left, ">=");
        return false;
    }
    
    if (Right.Type == Left.Type)
    {
        inspect_data_item GreaterThanResult = LeftInterface->GreaterThan(&Left, &Right);
        inspect_data_item EqualToResult = LeftInterface->Equals(&Left, &Right);
        *Result = NewBoolItem(GreaterThanResult.Bool || EqualToResult.Bool);
    }
    else
    {
        inspect_data_item RightCasted;
        inspect_data_operation_interface *RightInterface = GetInterface(Right.Type);
        if (!RightInterface->Cast(Left.Type, &Right, &RightCasted))
        {
            PrintInvalidCast(Left.Type, &Right, &RightToken);
            return false;
        }
        
        inspect_data_item GreaterThanResult = LeftInterface->LessThan(&Left, &Right);
        inspect_data_item EqualToResult = LeftInterface->Equals(&Left, &Right);
        *Result = NewBoolItem(GreaterThanResult.Bool || EqualToResult.Bool);
    }
    
    return true;
}

static
bool TryEvaluateLessThanOrEquals(write_parser *Parser,
                                 inspect_dict *Scope,
                                 inspect_data_item *Result)
{
    wtoken_info LeftToken = Current(Parser);
    inspect_data_item Left;
    if (!TryEvaluateGreaterThanOrEquals(Parser, Scope, &Left))
    {
        return false;
    }
    
    wtoken_info Next = Current(Parser);
    if (Next.Token.Type != WTokenType_LessThanOrEquals)
    {
        *Result = Left;
        return true;
    }
    
    wtoken_info RightToken;
    if (!PushToken(Parser, &RightToken))
    {
        return false;
    }
    
    inspect_data_item Right;
    if (!TryEvaluateParanthesis(Parser, Scope, &Right) &&
        !TryEvaluateLessThanOrEquals(Parser, Scope, &Right))
    {
        return false;
    }
    
    inspect_data_operation_interface *LeftInterface = GetInterface(Left.Type);
    
    if (!LeftInterface->CanExecuteOperation(LessThan_Op))
    {
        PrintInvalidOperation(&LeftToken, &Left, "<=");
        return false;
    }
    
    if (Right.Type == Left.Type)
    {
        inspect_data_item LessThanResult = LeftInterface->LessThan(&Left, &Right);
        inspect_data_item EqualToResult = LeftInterface->Equals(&Left, &Right);
        *Result = NewBoolItem(LessThanResult.Bool || EqualToResult.Bool);
    }
    else
    {
        inspect_data_item RightCasted;
        inspect_data_operation_interface *RightInterface = GetInterface(Right.Type);
        if (!RightInterface->Cast(Left.Type, &Right, &RightCasted))
        {
            PrintInvalidCast(Left.Type, &Right, &RightToken);
            return false;
        }
        
        inspect_data_item LessThanResult = LeftInterface->LessThan(&Left, &Right);
        inspect_data_item EqualToResult = LeftInterface->Equals(&Left, &Right);
        *Result = NewBoolItem(LessThanResult.Bool || EqualToResult.Bool);
    }
    
    return true;
}

static
bool TryEvaluateGreaterThan(write_parser *Parser,
                            inspect_dict *Scope,
                            inspect_data_item *Result)
{
    wtoken_info LeftToken = Current(Parser);
    inspect_data_item Left;
    if (!TryEvaluateLessThanOrEquals(Parser, Scope, &Left))
    {
        return false;
    }
    
    wtoken_info Next = Current(Parser);
    if (Next.Token.Type != WTokenType_GreaterThan)
    {
        *Result = Left;
        return true;
    }
    
    wtoken_info RightToken;
    if (!PushToken(Parser, &RightToken))
    {
        return false;
    }
    
    inspect_data_item Right;
    if (!TryEvaluateParanthesis(Parser, Scope, &Right) &&
        !TryEvaluateGreaterThan(Parser, Scope, &Right))
    {
        return false;
    }
    
    inspect_data_operation_interface *LeftInterface = GetInterface(Left.Type);
    
    if (!LeftInterface->CanExecuteOperation(GreaterThan_Op))
    {
        PrintInvalidOperation(&LeftToken, &Left, ">");
        return false;
    }
    
    if (Right.Type == Left.Type)
    {
        *Result = LeftInterface->GreaterThan(&Left, &Right);
    }
    else
    {
        inspect_data_item RightCasted;
        inspect_data_operation_interface *RightInterface = GetInterface(Right.Type);
        if (!RightInterface->Cast(Left.Type, &Right, &RightCasted))
        {
            PrintInvalidCast(Left.Type, &Right, &RightToken);
            return false;
        }
        
        *Result = LeftInterface->GreaterThan(&Left, &RightCasted);
    }
    
    return true;
}

static
bool TryEvaluateLessThan(write_parser *Parser,
                         inspect_dict *Scope,
                         inspect_data_item *Result)
{
    wtoken_info LeftToken = Current(Parser);
    inspect_data_item Left;
    if (!TryEvaluateGreaterThan(Parser, Scope, &Left))
    {
        return false;
    }
    
    wtoken_info Next = Current(Parser);
    if (Next.Token.Type != WTokenType_LessThan)
    {
        *Result = Left;
        return true;
    }
    
    wtoken_info RightToken;
    if (!PushToken(Parser, &RightToken))
    {
        return false;
    }
    
    inspect_data_item Right;
    if (!TryEvaluateParanthesis(Parser, Scope, &Right) &&
        !TryEvaluateLessThan(Parser, Scope, &Right))
    {
        return false;
    }
    
    inspect_data_operation_interface *LeftInterface = GetInterface(Left.Type);
    
    if (!LeftInterface->CanExecuteOperation(LessThan_Op))
    {
        PrintInvalidOperation(&LeftToken, &Left, "<");
        return false;
    }
    
    if (Right.Type == Left.Type)
    {
        *Result = LeftInterface->LessThan(&Left, &Right);
    }
    else
    {
        inspect_data_item RightCasted;
        inspect_data_operation_interface *RightInterface = GetInterface(Right.Type);
        if (!RightInterface->Cast(Left.Type, &Right, &RightCasted))
        {
            PrintInvalidCast(Left.Type, &Right, &RightToken);
            return false;
        }
        
        *Result = LeftInterface->LessThan(&Left, &RightCasted);
    }
    
    return true;
}

static
bool TryEvaluateEquality(write_parser *Parser,
                         inspect_dict *Scope,
                         inspect_data_item *Result)
{
    wtoken_info LeftToken = Current(Parser);
    inspect_data_item Left;
    if (!TryEvaluateLessThan(Parser, Scope, &Left))
    {
        return false;
    }
    
    wtoken_info Next = Current(Parser);
    if (Next.Token.Type != WTokenType_Equals)
    {
        *Result = Left;
        return true;
    }
    
    wtoken_info RightToken;
    if (!PushToken(Parser, &RightToken))
    {
        return false;
    }
    
    inspect_data_item Right;
    if (!TryEvaluateParanthesis(Parser, Scope, &Right) &&
        !TryEvaluateEquality(Parser, Scope, &Right))
    {
        return false;
    }
    
    inspect_data_operation_interface *LeftInterface = GetInterface(Left.Type);
    
    if (!LeftInterface->CanExecuteOperation(Equality_Op))
    {
        PrintInvalidOperation(&LeftToken, &Left, "==");
        return false;
    }
    
    if (Right.Type == Left.Type)
    {
        *Result = LeftInterface->Equals(&Left, &Right);
    }
    else
    {
        inspect_data_item RightCasted;
        inspect_data_operation_interface *RightInterface = GetInterface(Right.Type);
        if (!RightInterface->Cast(Left.Type, &Right, &RightCasted))
        {
            PrintInvalidCast(Left.Type, &Right, &RightToken);
            return false;
        }
        
        *Result = LeftInterface->Equals(&Left, &RightCasted);
    }
    
    return true;
}

static
bool TryEvaluateInEquality(write_parser *Parser,
                           inspect_dict *Scope,
                           inspect_data_item *Result)
{
    wtoken_info LeftToken = Current(Parser);
    inspect_data_item Left;
    if (!TryEvaluateEquality(Parser, Scope, &Left))
    {
        return false;
    }
    
    wtoken_info Next = Current(Parser);
    if (Next.Token.Type != WTokenType_NotEquals)
    {
        *Result = Left;
        return true;
    }
    
    wtoken_info RightToken;
    if (!PushToken(Parser, &RightToken))
    {
        return false;
    }
    
    inspect_data_item Right;
    if (!TryEvaluateParanthesis(Parser, Scope, &Right) &&
        !TryEvaluateInEquality(Parser, Scope, &Right))
    {
        return false;
    }
    
    inspect_data_operation_interface *LeftInterface = GetInterface(Left.Type);
    
    if (!LeftInterface->CanExecuteOperation(Equality_Op))
    {
        PrintInvalidOperation(&LeftToken, &Left, "!=");
        return false;
    }
    
    if (Right.Type == Left.Type)
    {
        *Result = LeftInterface->DoesNotEquals(&Left, &Right);
    }
    else
    {
        inspect_data_item RightCasted;
        inspect_data_operation_interface *RightInterface = GetInterface(Right.Type);
        if (!RightInterface->Cast(Left.Type, &Right, &RightCasted))
        {
            PrintInvalidCast(Left.Type, &Right, &RightToken);
            return false;
        }
        
        *Result = LeftInterface->DoesNotEquals(&Left, &RightCasted);
    }
    
    return true;
}

static
bool TryEvaluateBooleanOr(write_parser *Parser,
                          inspect_dict *Scope,
                          inspect_data_item *Result)
{
    wtoken_info LeftToken = Current(Parser);
    inspect_data_item Left;
    if (!TryEvaluateInEquality(Parser, Scope, &Left))
    {
        return false;
    }
    
    wtoken_info Next = Current(Parser);
    if (Next.Token.Type != WTokenType_BooleanOr)
    {
        *Result = Left;
        return true;
    }
    
    wtoken_info RightToken;
    if (!PushToken(Parser, &RightToken))
    {
        return false;
    }
    
    inspect_data_item Right;
    if (!TryEvaluateParanthesis(Parser, Scope, &Right) &&
        !TryEvaluateBooleanOr(Parser, Scope, &Right))
    {
        return false;
    }
    
    inspect_data_operation_interface *LeftInterface = GetInterface(Left.Type);
    
    if (!LeftInterface->CanExecuteOperation(BooleanOr_Op))
    {
        PrintInvalidOperation(&LeftToken, &Left, "||");
        return false;
    }
    
    if (Right.Type == Left.Type)
    {
        *Result = LeftInterface->BooleanOr(&Left, &Right);
    }
    else
    {
        inspect_data_item RightCasted;
        inspect_data_operation_interface *RightInterface = GetInterface(Right.Type);
        if (!RightInterface->Cast(Left.Type, &Right, &RightCasted))
        {
            PrintInvalidCast(Left.Type, &Right, &RightToken);
            return false;
        }
        
        *Result = LeftInterface->BooleanOr(&Left, &RightCasted);
    }
    
    return true;
}

static
bool TryEvaluateBooleanAnd(write_parser *Parser,
                           inspect_dict *Scope,
                           inspect_data_item *Result)
{
    wtoken_info LeftToken = Current(Parser);
    inspect_data_item Left;
    if (!TryEvaluateBooleanOr(Parser, Scope, &Left))
    {
        return false;
    }
    
    wtoken_info Next = Current(Parser);
    if (Next.Token.Type != WTokenType_BooleanAnd)
    {
        *Result = Left;
        return true;
    }
    
    wtoken_info RightToken;
    if (!PushToken(Parser, &RightToken))
    {
        return false;
    }
    
    inspect_data_item Right;
    if (!TryEvaluateParanthesis(Parser, Scope, &Right) &&
        !TryEvaluateBooleanAnd(Parser, Scope, &Right))
    {
        return false;
    }
    
    inspect_data_operation_interface *LeftInterface = GetInterface(Left.Type);
    
    if (!LeftInterface->CanExecuteOperation(BooleanAnd_Op))
    {
        PrintInvalidOperation(&LeftToken, &Left, "&&");
        return false;
    }
    
    if (Right.Type == Left.Type)
    {
        *Result = LeftInterface->BooleanAnd(&Left, &Right);
    }
    else
    {
        inspect_data_item RightCasted;
        inspect_data_operation_interface *RightInterface = GetInterface(Right.Type);
        if (!RightInterface->Cast(Left.Type, &Right, &RightCasted))
        {
            PrintInvalidCast(Left.Type, &Right, &RightToken);
            return false;
        }
        
        *Result = LeftInterface->BooleanAnd(&Left, &RightCasted);
    }
    
    return true;
}

static
bool TryEvaluateAssignment(write_parser *Parser,
                           inspect_dict *Scope,
                           inspect_data_item *Result)
{
    wtoken_info CurrentToken = Current(Parser);
    inspect_dict *AssignmentScope;
    std::string AssignmentName;
    
    inspect_data_item Item;
    if (TryEvaluateBooleanAnd(Parser, Scope, &Item))
    {
        CurrentToken = Current(Parser);
        
        if (CurrentToken.Token.Type != WTokenType_Assignment)
        {
            *Result = Item;
            return true;
        }
        
        if (Item.Owner == nullptr)
        {
            PrintLocation(CurrentToken.Line, CurrentToken.Column, CurrentToken.Filename);
            printf("Invalid Operator \"=\". Assignment only valid on L-Values\n");
            return false;
        }
        
        AssignmentName = FindLValueName(&Item);
        AssignmentScope = Item.Owner;
    }
    else if (CurrentToken.Token.Type == WTokenType_Identifier)
    {
        wtoken_info Next;
        if (!PushToken(Parser, &Next))
        {
            return false;
        }
        
        if (Next.Token.Type != WTokenType_Assignment)
        {
            PrintLocation(CurrentToken.Line, CurrentToken.Column, CurrentToken.Filename);
            printf("Unknown identifier \"%.*s\"\n",
                   (int)CurrentToken.Token.Length,
                   CurrentToken.Token.Text);
            return false;
        }
        
        AssignmentName = std::string(CurrentToken.Token.Text, CurrentToken.Token.Length);
        AssignmentScope = Scope;
    }
    else
    {
        return false;
    }
    
    if (!PushToken(Parser, &CurrentToken))
    {
        return false;
    }
    
    inspect_data_item NewValue;
    stack_frame NewFrame(Parser);
    if (!TryEvaluateSubExpression(Parser, Scope, &NewValue, &NewFrame))
    {
        return false;
    }
    
    *Result = CreateCopyOrReference(&NewValue);
    FreeIfExists(AssignmentScope, AssignmentName.c_str());
    Insert(AssignmentScope, AssignmentName.c_str(), Result);
    NewFrame.TryReleaseItem(Result); // NOTE(Brian): The item is now "owned" by the scope.
    return true;
}

static
bool TryEvaluateSubExpression(write_parser *Parser,
                              inspect_dict *Scope,
                              inspect_data_item *Result,
                              stack_frame *Frame)
{
    // NOTE(Brian): The Frame parameter is only there so that I remember to push a new
    // frame before this is called. (There may be a case in which we don't want to do that,
    // but I can't think of one.)
    REF(Frame);
    
    // TODO(Brian): Also evaluate for math and stuff.
    if (TryEvaluateAssignment(Parser, Scope, Result))
    {
        return true;
    }
    
    return false;
}

static
bool TryEvaluateIf(write_parser *Parser, inspect_dict *Scope)
{
    wtoken_info If = Current(Parser);
    if (If.Token.Type != WTokenType_If)
    {
        return false;
    }
    
    wtoken_info Next;
    if (!PushToken(Parser, &Next))
    {
        return false;
    }
    
    stack_frame NewFrame(Parser);
    inspect_data_item IfResult;
    if(!TryEvaluateSubExpression(Parser, Scope, &IfResult, &NewFrame))
    {
        PrintLocation(Next.Line, Next.Column, Next.Filename);
        printf("Expected expression.\n");
        return false;
    }
    else
    {
        if (IfResult.Type != Type_Bool)
        {
            PrintLocation(Next.Line, Next.Column, Next.Filename);
            printf("Expression does not evaluate to a bool\n");
            return false;
        }
        
        if (!IfResult.Bool)
        {
            return SkipPastMatchingEnd(Parser);
        }
        else
        {
            PushScopeLevel(Parser, false, true);
            if (!Evaluate(Parser, Scope, WTokenType_End))
            {
                return false;
            }
            PopScopeLevel(Parser, true);
            
            PushToken(Parser);
            return true;
        }
    }
}

static
bool ContinuePastToken(write_parser *Parser, write_token_type Type, const char *TokenString)
{
    wtoken_info CurrentToken = Current(Parser);
    while (CurrentToken.Token.Type != Type)
    {
        if (!PushToken(Parser, &CurrentToken))
        {
            return false;
        }
        
        if (CurrentToken.Token.Type == WTokenType_EOF)
        {
            PrintLocation(CurrentToken.Line, CurrentToken.Column, CurrentToken.Filename);
            printf("Expected \"%s\", found EOF\n", TokenString);
            return false;
        }
    }
    
    return PushToken(Parser);
}

static
bool ContinueToModeSwitch(write_parser *Parser)
{
    wtoken_info CurrentToken = Current(Parser);
    wtoken_info FirstToken = CurrentToken;
    while((CurrentToken.Flags & WTokenFlag_FirstAfterModeSwitch) == 0)
    {
        if (!PushToken(Parser, &CurrentToken))
        {
            return false;
        }
        
        if (CurrentToken.Token.Type == WTokenType_EOF)
        {
            PrintLocation(FirstToken.Line, FirstToken.Column, FirstToken.Filename);
            printf("Unexpected EOF\n");
            return false;
        }
    }
    
    return true;
}

static
bool TryEvaluateForLoopCondition(write_parser *Parser,
                                 inspect_dict *Scope,
                                 bool *Result)
{
    wtoken_info ExpressionBegin = Current(Parser);
    
    inspect_data_item Item;
    stack_frame NewFrame(Parser);
    if (!TryEvaluateSubExpression(Parser, Scope, &Item, &NewFrame))
    {
        return false;
    }
    
    if (Item.Type != Type_Bool)
    {
        PrintLocation(ExpressionBegin.Line, ExpressionBegin.Column, ExpressionBegin.Filename);
        printf("Expression must evaluate to a boolean value\n");
        return false;
    }
    
    *Result = Item.Bool;
    return true;
}

static
bool TryEvaluateForLoopIncrement(write_parser *Parser,
                                 inspect_dict *Scope)
{
    stack_frame NewFrame(Parser);
    inspect_data_item Item;
    if (!TryEvaluateSubExpression(Parser, Scope, &Item, &NewFrame))
    {
        return false;
    }
    
    return true;
}

static inline
bool CompareITokenAndWToken(inspect_token *Inspect, write_token *Write)
{
    if (Inspect->Length != Write->Length)
    {
        return false;
    }
    
    for (size_t I = 0; I < Inspect->Length; ++I)
    {
        if (Inspect->Text[I] != Write->Text[I])
        {
            return false;
        }
    }
    
    return true;
}

static
bool ItemHasAttribute(inspect_data_item *Item,
                      wtoken_info *Attribute)
{
    if (Item->Attributes == nullptr)
    {
        return false;
    }
    
    for (attribute_instance &Instance : Item->Attributes->Attributes)
    {
        if (Instance.Aliased)
        {
            if (CompareITokenAndWToken(&Instance.Alias->IdentifierToken.Token,
                                       &Attribute->Token))
            {
                return true;
            }
        }
        else
        {
            if (CompareITokenAndWToken(&Instance.IdentifierToken.Token,
                                       &Attribute->Token))
            {
                return true;
            }
        }
    }
    
    return false;
}

static
bool TryEvaluateHasAttribute(write_parser *Parser,
                             inspect_dict *Scope,
                             inspect_data_item *Result)
{
    wtoken_info HasAttributeName = Current(Parser);
    if (HasAttributeName.Token.Type != WTokenType_HasAttribute)
    {
        return false;
    }
    
    wtoken_info CurrentToken;
    if (!PushToken(Parser, &CurrentToken))
    {
        return false;
    }
    
    if (CurrentToken.Token.Type != WTokenType_LeftParen)
    {
        PrintLocation(CurrentToken.Line, CurrentToken.Column, CurrentToken.Filename);
        printf("Expected \"(\"\n");
        return false;
    }
    
    if (!PushToken(Parser))
    {
        return false;
    }
    
    stack_frame NewFrame(Parser);
    inspect_data_item Item;
    if (!TryEvaluateSubExpression(Parser, Scope, &Item, &NewFrame))
    {
        return false;
    }
    
    CurrentToken = Current(Parser);
    if (CurrentToken.Token.Type != WTokenType_Comma)
    {
        PrintLocation(CurrentToken.Line, CurrentToken.Column, CurrentToken.Filename);
        printf("Expected \",\"\n");
        return false;
    }
    
    wtoken_info StringToken;
    if (!PushToken(Parser, &StringToken))
    {
        return false;
    }
    
    if (StringToken.Token.Type != WTokenType_String)
    {
        PrintLocation(StringToken.Line,
                      StringToken.Column,
                      StringToken.Filename);
        printf("Expected string literal, found \"%.*s\"\n",
               (int)StringToken.Token.Length,
               StringToken.Token.Text);
        return false;
    }
    
    if (!PushToken(Parser, &CurrentToken))
    {
        return false;
    }
    
    if (CurrentToken.Token.Type != WTokenType_RightParen)
    {
        PrintLocation(CurrentToken.Line, CurrentToken.Column, CurrentToken.Filename);
        printf("Expected \")\"\n");
        return false;
    }
    
    *Result = NewBoolItem(ItemHasAttribute(&Item, &StringToken));
    return PushToken(Parser);
}

static
bool TryEvaluateProcedureCall(write_parser *Parser,
                              inspect_dict *Scope,
                              inspect_data_item *Result)
{
    wtoken_info Identifier = Current(Parser);
    if (Identifier.Token.Type != WTokenType_Identifier)
    {
        return false;
    }
    
    wtoken_info CurrentToken;
    if (!PushToken(Parser, &CurrentToken))
    {
        return false;
    }
    
    if (CurrentToken.Token.Type != WTokenType_LeftParen)
    {
        PopTokens(Parser, 1);
        return false;
    }
    
    if (!PushToken(Parser, &CurrentToken))
    {
        return false;
    }
    
    inspect_data_item ProcedureItem;
    if (!Lookup(Scope, &Identifier.Token, &ProcedureItem))
    {
        PrintLocation(Identifier.Line, Identifier.Column, Identifier.Filename);
        printf("Could not find procedure \"%.*s\"\n",
               (int)Identifier.Token.Length,
               Identifier.Token.Text);
        return false;
    }
    
    inspect_procedure &Procedure = *ProcedureItem.Procedure;
    
    inspect_data_item ProcedureScopeItem = NewDictItem();
    inspect_dict &ProcedureScope = *ProcedureScopeItem.Dict;
    ProcedureScope.Parent = Procedure.ParentScope;
    
    if (Procedure.Args.size() > 0)
    {
        for (int i = 0; i < (int)Procedure.Args.size(); ++i)
        {
            if (CurrentToken.Token.Type == WTokenType_RightParen)
            {
                FreeDataItem(&ProcedureScopeItem);
                PrintLocation(CurrentToken.Line, CurrentToken.Column, CurrentToken.Filename);
                printf ("Call to %.*s requires %i arguments, but was given %i\n",
                        (int)Identifier.Token.Length,
                        Identifier.Token.Text,
                        (int)Procedure.Args.size(),
                        i);
                return false;
            }
            
            stack_frame NewFrame(Parser);
            inspect_data_item Argument;
            if (!TryEvaluateSubExpression(Parser, Scope, &Argument, &NewFrame))
            {
                FreeDataItem(&ProcedureScopeItem);
                return false;
            }
            
            CurrentToken = Current(Parser);
            
            Insert(&ProcedureScope, &Procedure.Args[(uint64)i].Token, &Argument);
            
            if (i != (int)(Procedure.Args.size() - 1))
            {
                if (CurrentToken.Token.Type != WTokenType_Comma)
                {
                    FreeDataItem(&ProcedureScopeItem);
                    PrintLocation(CurrentToken.Line, CurrentToken.Column, CurrentToken.Filename);
                    printf("Expected \",\"\n");
                    return false;
                }
                
                if (!PushToken(Parser))
                {
                    return false;
                }
            }
            else
            {
                if (CurrentToken.Token.Type != WTokenType_RightParen)
                {
                    FreeDataItem(&ProcedureScopeItem);
                    PrintLocation(CurrentToken.Line, CurrentToken.Column, CurrentToken.Filename);
                    printf("Too many args for call to %.*s, expected %i\n",
                           (int)Identifier.Token.Length,
                           Identifier.Token.Text,
                           (int)Procedure.Args.size());
                    return false;
                }
            }
        }
    }
    else
    {
        if (CurrentToken.Token.Type != WTokenType_RightParen)
        {
            FreeDataItem(&ProcedureScopeItem);
            PrintLocation(CurrentToken.Line, CurrentToken.Column, CurrentToken.Filename);
            printf("Too many args for call to %.*s, expected %i\n",
                   (int)Identifier.Token.Length,
                   Identifier.Token.Text,
                   (int)Procedure.Args.size());
            return false;
        }
    }
    
    int ReturnLocation = Parser->Stack.Top;
    
    Jump(Parser, Procedure.BodyLocation);
    parser_state SavedState = SaveParserInfo(Parser);
    // RestoreTabState(Parser, &Procedure.TabState);
    PushScopeLevel(Parser, false, false);
    Parser->TabsToAdd += Procedure.TabState.TabsToAdd;
    Parser->TabsToRemove += Procedure.TabState.TabsToRemove;
    if (!Evaluate(Parser, &ProcedureScope, WTokenType_End))
    {
        FreeDataItem(&ProcedureScopeItem);
        return false;
    }
    Parser->TabsToAdd -= Procedure.TabState.TabsToAdd;
    Parser->TabsToRemove -= Procedure.TabState.TabsToRemove;
    PopScopeLevel(Parser, false);
    RestoreParserInfo(Parser, &SavedState);
    
    Jump(Parser, ReturnLocation);
    
    *Result = NewVoidItem();
    
    FreeDataItem(&ProcedureScopeItem);
    return PushToken(Parser);
}

static
bool TryEvaluateForLoop(write_parser *Parser, inspect_dict *Scope)
{
    wtoken_info For = Current(Parser);
    if (For.Token.Type != WTokenType_For)
    {
        return false;
    }
    
    wtoken_info Next;
    if (!PushToken(Parser, &Next))
    {
        return false;
    }
    
    inspect_data_item LocalScopeItem = NewDictItem();
    inspect_dict &LocalScope = *LocalScopeItem.Dict;
    LocalScope.Parent = Scope;
    
    stack_frame Frame(Parser);
    inspect_data_item Item;
    if (!TryEvaluateSubExpression(Parser, &LocalScope, &Item, &Frame))
    {
        FreeDataItem(&LocalScopeItem);
        return false;
    }
    
    Next = Current(Parser);
    if (Next.Token.Type != WTokenType_SemiColon)
    {
        PrintLocation(Next.Line, Next.Column, Next.Filename);
        printf("Expected \";\"\n");
        FreeDataItem(&LocalScopeItem);
        return false;
    }
    
    if (!PushToken(Parser, &Next))
    {
        FreeDataItem(&LocalScopeItem);
        return false;
    }
    
    int ConditionLocation = Parser->Stack.Top;
    if (!ContinuePastToken(Parser, WTokenType_SemiColon, ";"))
    {
        FreeDataItem(&LocalScopeItem);
        return false;
    }
    
    int IncrementerLocation = Parser->Stack.Top;
    
    if (!ContinueToModeSwitch(Parser))
    {
        PrintLocation(Next.Line, Next.Column, Next.Filename);
        printf("Could not find body of for loop\n");
        FreeDataItem(&LocalScopeItem);
        return false;
    }
    
    int BodyLocation = Parser->Stack.Top;
    
    for (;;)
    {
        Jump(Parser, ConditionLocation);
        bool ConditionTrue;
        if (!TryEvaluateForLoopCondition(Parser, &LocalScope, &ConditionTrue))
        {
            FreeDataItem(&LocalScopeItem);
            return false;
        }
        
        if (!ConditionTrue)
        {
            break;
        }
        
        Jump(Parser, BodyLocation);
        PushScopeLevel(Parser, false, true);
        if (!Evaluate(Parser, &LocalScope, WTokenType_End))
        {
            FreeDataItem(&LocalScopeItem);
            return false;
        }
        PopScopeLevel(Parser, true);
        
        Jump(Parser, IncrementerLocation);
        if (!TryEvaluateForLoopIncrement(Parser, &LocalScope))
        {
            FreeDataItem(&LocalScopeItem);
            return false;
        }
    }
    
    FreeDataItem(&LocalScopeItem);
    Jump(Parser, BodyLocation);
    SkipPastMatchingEnd(Parser);
    return true;
}

static
bool TryEvaluateForEach(write_parser *Parser, inspect_dict *Scope)
{
    wtoken_info For = Current(Parser);
    if (For.Token.Type != WTokenType_ForEach)
    {
        return false;
    }
    
    wtoken_info Variable;
    if (!PushToken(Parser, &Variable))
    {
        return false;
    }
    
    if (Variable.Token.Type == WTokenType_EOF) HandleUnexpectedEnd(&Variable);
    if (Variable.Token.Type != WTokenType_Identifier)
    {
        PrintLocation(Variable.Line, Variable.Column, Variable.Filename);
        printf("Expected identifier.\n");
        return false;
    }
    
    wtoken_info In;
    if (!PushToken(Parser, &In))
    {
        return false;
    }
    
    if (In.Token.Type == WTokenType_EOF) HandleUnexpectedEnd(&In);
    if (In.Token.Type != WTokenType_In)
    {
        PrintLocation(In.Line, In.Column, In.Filename);
        printf("Expected \"in\".");
        return false;
    }
    
    inspect_data_item ListItem;
    wtoken_info Next;
    if (!PushToken(Parser, &Next))
    {
        return false;
    }
    
    {
        stack_frame Frame(Parser);
        if (Next.Token.Type == WTokenType_EOF) HandleUnexpectedEnd(&In);
        if (!TryEvaluateSubExpression(Parser, Scope, &ListItem, &Frame))
        {
            return false;
        }
    }
    
    if (ListItem.Type != Type_List)
    {
        wtoken_info ListToken = Parser->Stack.Tokens[Parser->Stack.Top - 1];
        PrintLocation(ListToken.Line, ListToken.Column, ListToken.Filename);
        printf("Expression did not evaluate to a list.");
        return false;
    }
    
    if (ListItem.List->size() == 0)
    {
        return SkipPastMatchingEnd(Parser);
    }
    
    inspect_data_item LocalScopeItem = NewDictItem();
    inspect_dict &LocalScope = *LocalScopeItem.Dict;
    LocalScope.Parent = Scope;
    
    int Return = Parser->Stack.Top;
    for (size_t i = 0; i < ListItem.List->size(); ++i)
    {
        inspect_data_item Item = ListItem.List->at(i);
        
        Insert(&LocalScope, &Variable.Token, &Item);
        PushScopeLevel(Parser, false, true);
        if (!Evaluate(Parser, &LocalScope, WTokenType_End))
        {
            return false;
        }
        PopScopeLevel(Parser, true);
        
        wtoken_info CurrentToken = Current(Parser);
        if (CurrentToken.Token.Type == WTokenType_EOF)
        {
            HandleUnexpectedEnd(&CurrentToken);
            return false;
        }
        
        if (i != ListItem.List->size() - 1)
        {
            // Only jump back to the beginning of the loop if this wasn't
            // the last item.
            Jump(Parser, Return);
        }
    }
    
    FreeDataItem(&LocalScopeItem);
    return PushToken(Parser);
}

static
void CommitTextForAdjustment(write_parser *Parser, const char *Format, ...);

static
bool TryEvaluateWriteout(write_parser *Parser, inspect_dict *Scope)
{
    wtoken_info CurrentToken = Current(Parser);
    stack_frame NewFrame(Parser);
    inspect_data_item RefValue;
    if (TryEvaluateSubExpression(Parser, Scope, &RefValue, &NewFrame))
    {
        if (RefValue.Type == Type_String)
        {
            // Should write to file here
            CommitTextForAdjustment(Parser, RefValue.String);
            return true;
        }
        else if (RefValue.Type == Type_Int)
        {
            CommitTextForAdjustment(Parser, "%i", RefValue.Int);
            return true;
        }
        else if (RefValue.Type == Type_Bool)
        {
            CommitTextForAdjustment(Parser, RefValue.Bool ? "True" : "False");
            return true;
        }
        else if (RefValue.Type == Type_Void)
        {
            return true;
        }
        else
        {
            PrintLocation(CurrentToken.Line, CurrentToken.Column, CurrentToken.Filename);
            printf("Reference cannot be converted to a string.\n");
            return false;
        }
    }
    
    return false;
}

static
bool TryEvaluateIgnoreNewLine(write_parser *Parser)
{
    wtoken_info CurrentToken = Current(Parser);
    if (CurrentToken.Token.Type == WTokenType_IgnoreNewLine)
    {
        wtoken_info NewLine;
        if (!PushToken(Parser, &NewLine))
        {
            return false;
        }
        
        if (NewLine.Token.Type != WTokenType_TextNewLine)
        {
            PopTokens(Parser, 1);
        }
        else
        {
            SetIgnoreLineBeginTabState(Parser);
        }
        
        return PushToken(Parser);
    }
    else
    {
        return false;
    }
}

static
bool TryEvaluateDefinitionsBlock(write_parser *Parser, inspect_dict *Scope)
{
    wtoken_info CurrentToken = Current(Parser);
    if (CurrentToken.Token.Type != WTokenType_Definitions)
    {
        return false;
    }
    
    if (!PushToken(Parser))
    {
        return false;
    }
    
    PushScopeLevel(Parser, true, true);
    if (!Evaluate(Parser, Scope, WTokenType_End))
    {
        return false;
    }
    PopScopeLevel(Parser, true);
    
    if (!PushToken(Parser))
    {
        return false;
    }
    
    return true;
}

static
bool TryEvaluateBeginTab(write_parser *Parser, inspect_dict *Scope)
{
    wtoken_info CurrentToken = Current(Parser);
    if (CurrentToken.Token.Type != WTokenType_BeginTab)
    {
        return false;
    }
    
    if (!PushToken(Parser))
    {
        return false;
    }
    
    PushScopeLevel(Parser, false, true);
    ++Parser->TabsToAdd;
    if (!Evaluate(Parser, Scope, WTokenType_End))
    {
        return false;
    }
    --Parser->TabsToAdd;
    PopScopeLevel(Parser, true);
    
    if (!PushToken(Parser))
    {
        return false;
    }
    
    return true;
}

// NOTE(Brian): a $ breakpoint $ is basically a no-op, but I use it to
// with a debugger to break into specific locations while parsing.
static
bool TryEvaluateBreakpoint(write_parser *Parser)
{
    wtoken_info CurrentToken = Current(Parser);
    if (CurrentToken.Token.Type != WTokenType_Breakpoint)
    {
        return false;
    }
    
    return PushToken(Parser);
}

static
bool EvaluateExpression(write_parser *Parser, inspect_dict *Scope)
{
    if (TryEvaluateDefine(Parser, Scope))
    {
        return true;
    }
    
    if (TryEvaluateForEach(Parser, Scope))
    {
        return true;
    }
    
    if (TryEvaluateForLoop(Parser, Scope))
    {
        return true;
    }
    
    if (TryEvaluateIf(Parser, Scope))
    {
        return true;
    }
    
    if (TryEvaluateIgnoreNewLine(Parser))
    {
        return true;
    }
    
    if (TryEvaluateDefinitionsBlock(Parser, Scope))
    {
        return true;
    }
    
    if (TryEvaluateBeginTab(Parser, Scope))
    {
        return true;
    }
    
    if (TryEvaluateBreakpoint(Parser))
    {
        return true;
    }
    
    // NOTE(Brian): This has to come last.
    wtoken_info CurrentToken = Current(Parser);
    if (TryEvaluateWriteout(Parser, Scope))
    {
        return true;
    }
    
    return false;
}

static inline
void SetLineBeginTabState(write_parser *Parser)
{
    Parser->TabsRemoved = 0;
    Parser->TabsAdded = 0;
    Parser->Flags |= WP_ShouldAdjustTabs;
    Parser->QueuedTabs = 0;
}

static inline
void SetIgnoreLineBeginTabState(write_parser *Parser)
{
    Parser->TabsRemoved = 0;
    Parser->Flags |= WP_ShouldAdjustTabs;
    Parser->QueuedTabs = 0;
}

#if 0
static inline
tab_state SaveTabState(write_parser *Parser)
{
    tab_state State;
    State.TabsToAdd = Parser->TabsToAdd;
    State.TabsToRemove = Parser->TabsToRemove;
    State.TabsToAdd = 0;
    State.TabsToRemove = 0;
    
    return State;
}

static inline
void RestoreTabState(write_parser *Parser, tab_state *State)
{
    Parser->TabsToAdd = State->TabsToAdd;
    Parser->TabsToRemove = State->TabsToRemove;
}
#endif

static inline
parser_state SaveParserInfo(write_parser *Parser)
{
    parser_state Result;
    Result.AutoClearNewLineStack = Parser->AutoClearNewLineStack;
    Result.AutoClearNewLineTop = Parser->AutoClearNewLineTop;
    Parser->AutoClearNewLineTop = 0;
    Parser->AutoClearNewLineStack = 0;
    
    // Result.TabState = SaveTabState(Parser);
    
    return Result;
}

static inline
void RestoreParserInfo(write_parser *Parser, parser_state *State)
{
    Parser->AutoClearNewLineStack = State->AutoClearNewLineStack;
    Parser->AutoClearNewLineTop = State->AutoClearNewLineTop;
    
    // RestoreTabState(Parser, &State->TabState);
}

static inline
bool ShouldIgnoreNewLine(write_parser *Parser)
{
    return Parser->AutoClearNewLineStack & (1llu << Parser->AutoClearNewLineTop);
}

static inline
void PushScopeLevel(write_parser *Parser,
                    bool IgnoreNewLines,
                    bool IncreaseTabLevel)
{
    ++Parser->AutoClearNewLineTop;
    Parser->AutoClearNewLineStack |= (uint64)IgnoreNewLines << Parser->AutoClearNewLineTop;
    
    if (IncreaseTabLevel)
    {
        Parser->TabsToRemove++;
    }
}

static inline
void PopScopeLevel(write_parser *Parser,
                   bool PopTabLevel)
{
    Parser->AutoClearNewLineStack &= ~(1llu << Parser->AutoClearNewLineTop);
    Parser->AutoClearNewLineTop--;
    
    if (PopTabLevel)
    {
        Parser->TabsToRemove--;
    }
}

static inline
void StopEatingTabs(write_parser *Parser)
{
    Parser->Flags &= ~WP_ShouldAdjustTabs;
}

static inline
uint32 ShouldAdjustTabs(write_parser *Parser)
{
    return Parser->Flags & WP_ShouldAdjustTabs;
}

static inline
const char *EatTab(write_parser *Parser, const char *String)
{
    const char *Result = String + 1;
    
    ++Parser->TabsRemoved;
    return Result;
}

static inline
const char *EatSpaces(write_parser *Parser, const char *String)
{
    const char *Result = String;
    
    for (int32 I = 0; I < Parser->TabSize; ++I)
    {
        if (String[I] != ' ')
        {
            return String;
        }
    }
    
    Result += Parser->TabSize;
    ++Parser->TabsRemoved;
    return Result;
}

static inline
void AddTabs(write_parser *Parser, int32 RequiredTabs)
{
    char OutputChar;
    if (Parser->Flags & WP_UseSpacesInsteadOfTabs)
    {
        OutputChar = ' ';
    }
    else
    {
        OutputChar = '\t';
    }
    
    for (int32 I = 0; I < RequiredTabs * Parser->TabSize; ++I)
    {
        fputc(OutputChar, Parser->Output);
    }
    
    Parser->TabsAdded += RequiredTabs;
}

static inline
const char *AdjustTab(write_parser *Parser, const char *Output)
{
    if (ShouldAdjustTabs(Parser))
    {
        if (Parser->QueuedTabs)
        {
            int32 TabsToRemove = Parser->TabsToRemove - Parser->TabsRemoved;
            
            if (Parser->QueuedTabs == TabsToRemove)
            {
                Parser->QueuedTabs = 0;
                Parser->TabsRemoved = Parser->TabsToRemove;
            }
            else if (Parser->QueuedTabs < TabsToRemove)
            {
                Parser->TabsRemoved += Parser->QueuedTabs;
                Parser->QueuedTabs = 0;
            }
            else if (Parser->QueuedTabs > TabsToRemove)
            {
                Parser->QueuedTabs -= TabsToRemove;
                Parser->TabsRemoved = Parser->TabsToRemove;
                
                AddTabs(Parser, Parser->QueuedTabs);
                Parser->QueuedTabs = 0;
            }
        }
        
        if (Parser->TabsAdded != Parser->TabsToAdd)
        {
            AddTabs(Parser, Parser->TabsToAdd - Parser->TabsAdded);
        }
        
        if (Parser->TabsRemoved != Parser->TabsToRemove)
        {
            for (int32 I = Parser->TabsRemoved; I < Parser->TabsToRemove; ++I)
            {
                if (*Output == '\t')
                {
                    Output = EatTab(Parser, Output);
                }
                else if (*Output == ' ')
                {
                    Output = EatSpaces(Parser, Output);
                }
            }
        }
        
        if (*Output != '\t' || *Output != ' ')
        {
            StopEatingTabs(Parser);
        }
    }
    
    return Output;
}

static inline
void CommitTextForAdjustment(write_parser *Parser, const char *Format, ...)
{
    va_list Arguments;
    va_start(Arguments, Format);
    
    int OutputSize = vsnprintf(Parser->OutputBuffer, Parser->OutputBufferSize,
                               Format, Arguments);
    
    if ((size_t)OutputSize > Parser->OutputBufferSize)
    {
        free(Parser->OutputBuffer);
        Parser->OutputBufferSize = (size_t)OutputSize;
        Parser->OutputBuffer = (char *)malloc(Parser->OutputBufferSize);
        vsnprintf(Parser->OutputBuffer, Parser->OutputBufferSize,
                  Format, Arguments);
    }
    
    va_end(Arguments);
    
    const char *Output = AdjustTab(Parser, Parser->OutputBuffer);
    
    if (Parser->Flags & WP_UseSpacesInsteadOfTabs)
    {
        for (const char *C = Output; *C; ++C)
        {
            if (*C == '\t')
            {
                for (int i = 0; i < Parser->TabSize; ++i)
                {
                    fputc(' ', Parser->Output);
                }
            }
            else
            {
                fputc(*C, Parser->Output);
            }
        }
    }
    else
    {
        fputs(Output, Parser->Output);
    }
}

static inline
int32 Tabs(write_parser *Parser, wtoken_info *Token)
{
    int32 Spaces = 0;
    int32 Tabs = 0;
    for (size_t I = 0; I < Token->Token.Length; ++I)
    {
        char C = Token->Token.Text[I];
        if (C == '\t')
        {
            ++Tabs;
        }
        else if (C == ' ')
        {
            ++Spaces;
        }
        else
        {
            return 0;
        }
    }
    
    return Tabs + (Spaces / Parser->TabSize);
}

static
bool Evaluate(write_parser *Parser, inspect_dict *Scope, write_token_type Until)
{
    for(;;)
    {
        wtoken_info CurrentToken = Current(Parser);
        
        if (CurrentToken.Token.Type == Until)
        {
            return true;
        }
        
        if (CurrentToken.Token.Type == WTokenType_Text)
        {
            int TabCount = Tabs(Parser, &CurrentToken);
            if (!TabCount)
            {
                CommitTextForAdjustment(Parser, "%.*s", (int)CurrentToken.Token.Length,
                                        CurrentToken.Token.Text);
            }
            else
            {
                // Queue these tabs for later.
                // We only want to output the tabs if they proceed text.
                Parser->QueuedTabs += TabCount;
            }
            
            wtoken_info Next;
            if (!PushToken(Parser, &Next))
            {
                return false;
            }
        }
        else if (CurrentToken.Token.Type == WTokenType_TextNewLine)
        {
            SetLineBeginTabState(Parser);
            
            if (!ShouldIgnoreNewLine(Parser))
            {
                fputc('\n', Parser->Output);
            }
            
            wtoken_info Next;
            if (!PushToken(Parser, &Next))
            {
                return false;
            }
        }
        else
        {
            if (!EvaluateExpression(Parser, Scope))
            {
                if (!(Parser->Flags & WP_IllegalExpressionReported))
                {
                    PrintLocation(CurrentToken.Line, CurrentToken.Column, CurrentToken.Filename);
                    printf("Illegal expression\n");
                    Parser->Flags |= WP_IllegalExpressionReported;
                }
                
                return false;
            }
        }
    }
}

bool EvaluateTemplate(write_parser *Parser, inspect_dict *Scope)
{
    // Make the current token the first token.
    wtoken_info Next;
    if (!PushToken(Parser, &Next))
    {
        return false;
    }
    
    if (!Evaluate(Parser, Scope, WTokenType_EOF))
    {
        return false;
    }
    
    return true;
}
