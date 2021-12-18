
#include <assert.h>
#include <stdlib.h>
#include <algorithm>

#include "codegen_lex_base.h"
#include "codegen_lex_inspect.h"
#include "codegen_parse_base.h"
#include "codegen_parse_inspect.h"
#include "codegen_inspect_data.h"

static inline
void CreateLexerStack(lexer_stack *Stack)
{
    Stack->Top = -1;
}

static inline
void PushLexer(inspect_parser *Parser, inspect_lexer *Lexer)
{
    ++Parser->LexerStack.Top;
    assert(Parser->LexerStack.Top < lexer_stack::Max);
    
    Parser->LexerStack.Lexers[Parser->LexerStack.Top] = { Parser->At, Parser->Lexer };
    Parser->Lexer = Lexer;
}

static inline
bool PopLexer(lexer_stack *Stack, lex_state *State)
{
    if (Stack->Top >= 0)
    {
        *State = Stack->Lexers[Stack->Top--];
        return true;
    }
    
    return false;
}

static inspect_data_item NewTypeItem(type *Type);

static inline
inspect_data_item NewTypeArgsItem(type_args *Args)
{
    inspect_data_item ArgsItem = NewListItem();
    inspect_list &ArgsList = *ArgsItem.List;
    
    for (type &Type : Args->Args)
    {
        ArgsList.push_back(NewTypeItem(&Type));
    }
    
    return ArgsItem;
}

static inline
std::string GetFullTypeNameForPointer(type *Type)
{
    std::string Result;
    
    type *Current = Type;
    for (;;)
    {
        if (Current->IsPointer)
        {
            Result.insert(0, "*");
        }
        else if (Current->IsReference)
        {
            Result.insert(0, "&");
        }
        else
        {
            Result.insert(0, " ");
            Result.insert(0, Current->TypeName.Token.Text, Current->TypeName.Token.Length);
            break;
        }
        
        Current = Current->InnerType;
    }
    
    return Result;
}

static
inspect_data_item NewTypeItem(type *Type)
{
    inspect_data_item TypeItem = NewDictItem();
    inspect_dict &TypeDict = *TypeItem.Dict;
    
    if (Type->IsPointer || Type->IsReference)
    {
        Insert(&TypeDict, "Name", NewStringItem(GetFullTypeNameForPointer(Type).c_str()));
    }
    else
    {
        Insert(&TypeDict, "Name", NewStringItem(&Type->TypeName));
    }
    
    Insert(&TypeDict, "IsPointer", NewBoolItem(Type->IsPointer));
    Insert(&TypeDict, "IsReference", NewBoolItem(Type->IsReference));
    
    if (Type->InnerType)
    {
        Insert(&TypeDict, "HasInnerType", NewBoolItem(true));
        Insert(&TypeDict, "InnerType", NewTypeItem(Type->InnerType));
    }
    else
    {
        Insert(&TypeDict, "HasInnerType", NewBoolItem(false));
    }
    
    Insert(&TypeDict, "Args", NewTypeArgsItem(&Type->Args));
    
    TypeItem.OptionalSourceToken = Type->TypeName;
    
    return TypeItem;
}

static inline
inspect_data_item CreateTypedArgumentItem(typed_argument_declaration *Item)
{
    inspect_data_item ArgumentItem = NewDictItem();
    inspect_dict *Dict = ArgumentItem.Dict;
    
    Insert(Dict, "Name", NewStringItem(&Item->Name));
    Insert(Dict, "Type", NewTypeItem(&Item->Type));
    
    return ArgumentItem;
}

static inline
inspect_data_item CreateTypedArgumentListItem(typed_argument_list_declaration *List)
{
    inspect_data_item ListItem = NewListItem();
    inspect_list *ListData = ListItem.List;
    
    for (typed_argument_declaration &Argument : List->Arguments)
    {
        ListData->push_back(CreateTypedArgumentItem(&Argument));
    }
    
    return ListItem;
}

static inline
inspect_data_item CreateFieldItem(field *Field)
{
    inspect_data_item FieldItem = NewDictItem();
    inspect_dict &FieldDict = *FieldItem.Dict;
    
    Insert(&FieldDict, "Type", NewTypeItem(&Field->Type));
    Insert(&FieldDict, "Name", NewStringItem(&Field->Name));
    Insert(&FieldDict, "HasInitializer", NewBoolItem(Field->HasInitializer));
    
    if (Field->HasInitializer)
    {
        Insert(&FieldDict, "Initializer", NewStringItem(&Field->InitializerText));
    }
    else
    {
        Insert(&FieldDict, "Initializer", NewStringItem(""));
    }
    
    Insert(&FieldDict, "IsMethod", NewBoolItem(Field->IsMethod));
    if (Field->IsMethod)
    {
        Insert(&FieldDict, "MethodArguments", CreateTypedArgumentListItem(&Field->Arguments));
    }
    
    FieldItem.Attributes = Field->Attributes;
    
    return FieldItem;
}

static inline
inspect_data_item CreateTypeInfoItemInternal(inspect_data_item Name,
                                             inspect_data_item CamelCase,
                                             inspect_data_item Descriptor,
                                             attribute_list *Attributes)
{
    inspect_data_item TypeInfoItem = NewDictItem();
    inspect_dict &TypeDict = *TypeInfoItem.Dict;
    
    Insert(&TypeDict, "Name", &Name);
    Insert(&TypeDict, "Descriptor", &Descriptor);
    Insert(&TypeDict, "CamelCase", &CamelCase);
    TypeInfoItem.Attributes = Attributes;
    
    return TypeInfoItem;
}

static inline
char ToUppercase(char C)
{
    return C - 32;
}

static inline
char TryUppercase(char C)
{
    if (C >= 'a' && C <= 'z')
    {
        return ToUppercase(C);
    }
    
    return C;
}

static inline
char *NameToCamelCase(const char *Text, size_t Length)
{
    if (Length == 0)
    {
        char *Buffer = (char *)malloc(1);
        *Buffer = '\0';
        return Buffer;
    }
    
    char *Buffer = (char *)malloc(Length + 1);
    char *At = Buffer;
    size_t I = 0;
    if (Text[I] != '_')
    {
        *At++ = TryUppercase(Text[I]);
        ++I;
    }
    
    for (; I < Length; ++I)
    {
        if (Text[I] == '_' &&
            I + 1 < Length)
        {
            ++I;
            *At++ = TryUppercase(Text[I]);
        }
        else
        {
            *At++ = Text[I];
        }
    }
    
    *At = '\0';
    
    return Buffer;
}

static inline
char *NameToCamelCase(const char *String)
{
    return NameToCamelCase(String, strlen(String));
}

static inline
char *NameToCamelCase(itoken_info *Token)
{
    return NameToCamelCase(Token->Token.Text, Token->Token.Length);
}

static
inspect_data_item CreateStructItem(defined_struct *Struct,
                                   inspect_dict *TypeInfo,
                                   attribute_list *Attributes)
{
    inspect_data_item StructDictItem = NewDictItem();
    inspect_dict &StructDict = *StructDictItem.Dict;
    
    Insert(&StructDict, "Name", NewStringItem(&Struct->Identifier));
    
    inspect_data_item FieldListItem = NewListItem();
    inspect_list &FieldList = *FieldListItem.List;
    Insert(&StructDict, "Fields", &FieldListItem);
    
    for (field &Field : Struct->Fields)
    {
        FieldList.push_back(CreateFieldItem(&Field));
    }
    
    inspect_data_item FieldCountItem = NewIntItem((int)FieldList.size());
    Insert(&StructDict, "FieldCount", &FieldCountItem);
    
    Insert(&StructDict, "TypeInfo", CreateReference(TypeInfo));
    
    StructDictItem.Attributes = Attributes;
    
    return StructDictItem;
}

inline
inspect_data_item CreateTypeInfoItem(declared_type *Info, attribute_list *Attributes)
{
    return CreateTypeInfoItemInternal(NewStringItem(&Info->TypeName),
                                      ReceiveStringItem(NameToCamelCase(&Info->TypeName)),
                                      NewStringItem(&Info->DescriptorName),
                                      Attributes);
}

inline
inspect_data_item CreateTypeInfoItem(const char *Name,
                                     const char *DescriptorName,
                                     attribute_list *Attributes)
{
    return CreateTypeInfoItemInternal(NewStringItem(Name),
                                      ReceiveStringItem(NameToCamelCase(Name)),
                                      NewStringItem(DescriptorName),
                                      Attributes);
}

inline
inspect_data_item CreateTypeInfoItem(inspect_data_item Name,
                                     attribute_list *Attributes)
{
    char *CamelCase;
    if (Name.Type == Type_String)
    {
        CamelCase = NameToCamelCase(Name.String);
    }
    else
    {
        CamelCase = 0;
        assert(false);
    }
    
    char Descriptor[1024];
    snprintf(Descriptor, 1024, "%sTD", CamelCase);
    
    return CreateTypeInfoItemInternal(Name,
                                      ReceiveStringItem(CamelCase),
                                      NewStringItem(Descriptor),
                                      Attributes);
}

inline
inspect_data_item CreateTypeInfoItem(defined_struct *Struct, attribute_list *Attributes)
{
    return CreateTypeInfoItem(NewStringItem(&Struct->Identifier),
                              Attributes);
}

static
bool ReceiveNextToken(inspect_parser *Parser)
{
    Parser->Stack.Top++;
    if (Parser->Stack.Top < Parser->Stack.Populated)
    {
        Parser->At = Parser->Stack.Tokens[Parser->Stack.Top];
        return true;
    }
    
    if (Parser->Stack.Top > Parser->Stack.Capacity)
    {
        Resize(&Parser->Stack);
    }
    
    Parser->At.Token = NextToken(Parser->Lexer);
    Parser->At.Line = Parser->Lexer->Line;
    Parser->At.Column = Parser->Lexer->Column;
    Parser->At.Filename = Parser->Lexer->Filename;
    
    Parser->Stack.Tokens[Parser->Stack.Top] = Parser->At;
    ++Parser->Stack.Populated;
    
    if (Parser->At.Token.Type == ITokenType_IncompleteString)
    {
        PrintLocation(Parser->At.Line, Parser->At.Column, Parser->Lexer->Filename);
        printf("Incomplete string. (Are you missing a closing quote?)");
        return false;
    }
    
    return true;
}

static
void MoveBack(inspect_parser *Parser)
{
    Parser->Stack.Top--;
    Parser->At = Parser->Stack.Tokens[Parser->Stack.Top];
}

static
void MoveTo(inspect_parser *Parser, int Location)
{
    Parser->Stack.Top = Location;
    Parser->At = Parser->Stack.Tokens[Location];
}

static inline
bool CheckAt(inspect_parser *Parser, inspect_token_type Expected)
{
    return Parser->At.Token.Type == Expected;
}

static inline
bool CheckNext(inspect_parser *Parser, inspect_token_type Expected)
{
    return ReceiveNextToken(Parser) && Parser->At.Token.Type == Expected;
}

static inline
bool ExpectAt(inspect_parser *Parser, inspect_token_type Expected, const char *ExpectedString)
{
    if (Parser->At.Token.Type != Expected)
    {
        PrintLocation(Parser->At.Line, Parser->At.Column, Parser->Lexer->Filename);
        printf("Expected: \"%s\", Found: \"%.*s\"\n",
               ExpectedString, (int)Parser->At.Token.Length, Parser->At.Token.Text);
        
        return false;
    }
    
    return true;
}

static inline
bool ExpectNext(inspect_parser *Parser, inspect_token_type Expected, const char *ExpectedString)
{
    if (!ReceiveNextToken(Parser))
    {
        return false;
    }
    
    return ExpectAt(Parser, Expected, ExpectedString);
}

static bool TryParseType(inspect_parser *Parser, type *Result);

static
bool TryParseTypeArgs(inspect_parser *Parser, type_args *Result)
{
    if (!CheckAt(Parser, ITokenType_LeftAngle))
    {
        return false;
    }
    
    ReceiveNextToken(Parser);
    itoken_info ArgToken = Parser->At;
    while (ArgToken.Token.Type != ITokenType_RightAngle)
    {
        // TODO(Brian): Ewww, if we get our own list type for codegen,
        // we should have it return the element inserted.
        Result->Args.emplace_back();
        type &Argument = Result->Args.back();
        
        if (!TryParseType(Parser, &Argument))
        {
            Result->Args.pop_back();
            PrintLocation(ArgToken.Line, ArgToken.Column, ArgToken.Filename);
            printf("Unable to parse type\n");
            return false;
        }
        
        ArgToken = Parser->At;
    }
    
    // NOTE(Brian): All succesful Try* calls should leave the parser at the next token.
    // The above loop will not do that itself, so we need to manually do that here.
    ReceiveNextToken(Parser);
    
    return true;
}

static bool
FailAtBarrier(inspect_parser *Parser,
              int Barrier,
              const char *Message)
{
    if (Parser->Stack.Top == Barrier)
    {
        PrintLocation(Parser->At.Line, Parser->At.Column, Parser->At.Filename);
        puts(Message);
        return true;
    }
    
    return false;
}

static void
PrintUnexpectedToken(itoken_info *Token)
{
    PrintLocation(Token->Line, Token->Column, Token->Filename);
    printf("Unexpected token \"%.*s\"\n",
           (int)Token->Token.Length,
           Token->Token.Text);
}

static bool
FailAtBarrierWithUnexpectedToken(inspect_parser *Parser,
                                 int Barrier)
{
    if (Parser->Stack.Top == Barrier)
    {
        PrintUnexpectedToken(&Parser->At);
        return true;
    }
    
    return false;
}

static
bool TryParseTypeReverse(inspect_parser *Parser,
                         type *Result,
                         int Barrier);

static
bool TryParseTypeArgsReverse(inspect_parser *Parser,
                             type_args *Result,
                             int Barrier)
{
    if (!CheckAt(Parser, ITokenType_RightAngle))
    {
        return false;
    }
    
    do
    {
        MoveBack(Parser);
        if (FailAtBarrierWithUnexpectedToken(Parser, Barrier))
        {
            return false;
        }
        
        Result->Args.emplace_back();
        type &Argument = Result->Args.back();
        if (!TryParseTypeReverse(Parser, &Argument, Barrier))
        {
            return false;
        }
        
        if (!CheckAt(Parser, ITokenType_Comma) &&
            !CheckAt(Parser, ITokenType_LeftAngle))
        {
            PrintUnexpectedToken(&Parser->At);
            return false;
        }
    }
    while (!CheckAt(Parser, ITokenType_LeftAngle));
    
    std::reverse(Result->Args.begin(), Result->Args.end());
    MoveBack(Parser);
    return true;
}

static inline
void InitializeType(type *Type)
{
    Type->IsPointer = false;
    Type->IsReference = false;
    Type->InnerType = nullptr;
}

static
bool TryParseTypeReverse(inspect_parser *Parser,
                         type *Result,
                         int Barrier)
{
    InitializeType(Result);
    
    if (CheckAt(Parser, ITokenType_Asterisk))
    {
        Result->IsPointer = true;
        
        MoveBack(Parser);
        if (FailAtBarrierWithUnexpectedToken(Parser, Barrier))
        {
            return false;
        }
        
        Result->InnerType = new type;
        return TryParseTypeReverse(Parser, Result->InnerType, Barrier);
    }
    else if (CheckAt(Parser, ITokenType_Ampersand))
    {
        Result->IsReference = true;
        
        MoveBack(Parser);
        if (FailAtBarrierWithUnexpectedToken(Parser, Barrier))
        {
            return false;
        }
        
        Result->InnerType = new type;
        return TryParseTypeReverse(Parser, Result->InnerType, Barrier);
    }
    
    if (CheckAt(Parser, ITokenType_RightAngle))
    {
        if (!TryParseTypeArgsReverse(Parser, &Result->Args, Barrier))
        {
            return false;
        }
        
        if (FailAtBarrierWithUnexpectedToken(Parser, Barrier))
        {
            return false;
        }
    }
    
    if (!ExpectAt(Parser, ITokenType_Identifier, "Identifier"))
    {
        return false;
    }
    
    Result->TypeName = Parser->At;
    
    
    
    MoveBack(Parser);
    return true;
}

static
bool TryParseType(inspect_parser *Parser, type *Result)
{
    type Temporary;
    InitializeType(&Temporary);
    
    if (!CheckAt(Parser, ITokenType_Identifier))
    {
        return false;
    }
    Temporary.TypeName = Parser->At;
    
    // Type arguments are optional, so we should check first if there is a '<'
    if (CheckNext(Parser, ITokenType_LeftAngle))
    {
        if (!TryParseTypeArgs(Parser, &Temporary.Args))
        {
            return false;
        }
    }
    
    // Pointer and reference handling.
    for (;;)
    {
        if (CheckAt(Parser, ITokenType_Asterisk))
        {
            type *NewInner = new type;
            *NewInner = Temporary;
            
            Temporary.InnerType = NewInner;
            Temporary.IsPointer = true;
            Temporary.IsReference = false;
        }
        else if (CheckAt(Parser, ITokenType_Ampersand))
        {
            type *NewInner = new type;
            *NewInner = Temporary;
            
            Temporary.InnerType = NewInner;
            Temporary.IsPointer = false;
            Temporary.IsReference = true;
        }
    }
    
    *Result = Temporary;
    
    return true;
}

static inline
inspect_ctext CreateCText(itoken_info *Begin, itoken_info *End)
{
    char *BeginPtr;
    if (Begin->Token.Type == ITokenType_String)
    {
        // We start string tokens at the character after the first double-quote.
        // We want to inclue that in CTexts.
        BeginPtr = Begin->Token.Text - 1;
    }
    else
    {
        BeginPtr = Begin->Token.Text;
    }
    
    char *EndPtr = End->Token.Text;
    
    inspect_ctext Result;
    Result.Begin = BeginPtr;
    Result.Length = (size_t)EndPtr - (size_t)BeginPtr;
    return Result;
};

static
bool TryParseAttributeList(inspect_parser *Parser,
                           attribute_list **Result,
                           int Until = INT_MAX);

static
bool TryParseTypedArgumentListReverse(inspect_parser *Parser,
                                      typed_argument_list_declaration *Result,
                                      int Barrier);

static
bool TryParseField(inspect_parser *Parser,
                   field *Result)
{
    // Fields need to be parsed in reverse because if there
    // is an attribute list as the beginning of the field,
    // the name of a type and the name of an attribute are indistinguishable.
    Result->Attributes = nullptr;
    
    int Begin = Parser->Stack.Top;
    int Before = Begin - 1;
    int End;
    
    itoken_info FirstToken = Parser->At;
    
    for (;;)
    {
        if (CheckAt(Parser, ITokenType_SemiColon))
        {
            break;
        }
        else if (CheckAt(Parser, ITokenType_Equals))
        {
            break;
        }
        
        if (CheckAt(Parser, ITokenType_End))
        {
            PrintLocation(FirstToken.Line, FirstToken.Column, FirstToken.Filename);
            printf("Found EOF while parsing field.\n");
            return false;
        }
        
        if (!ReceiveNextToken(Parser))
        {
            return false;
        }
    }
    
    End = Parser->Stack.Top;
    
    if (FailAtBarrier(Parser, Before, "Unexpected \";\"\n"))
    {
        return false;
    }
    
    itoken_info Current = Parser->At;
    MoveBack(Parser);
    
    if (CheckAt(Parser, ITokenType_RightParen))
    {
        if (!TryParseTypedArgumentListReverse(Parser, &Result->Arguments, Before))
        {
            return false;
        }
        
        Result->IsMethod = true;
    }
    else
    {
        Result->IsMethod = false;
    }
    
    if (FailAtBarrier(Parser, Before, "Unexpected \";\"\n"))
    {
        return false;
    }
    
    if (!ExpectAt(Parser, ITokenType_Identifier, "Identifier"))
    {
        return false;
    }
    
    itoken_info Identifier = Parser->At;
    MoveBack(Parser);
    
    if (FailAtBarrier(Parser, Before, "Unexpected Identifier.\n"))
    {
        return false;
    }
    
    Result->Name = Identifier;
    
    if (!TryParseTypeReverse(Parser, &Result->Type, Before))
    {
        return false;
    }
    
    if (Parser->Stack.Top != Before)
    {
        // There are some potential attributes that we want to parse.
        int EndOfAttributes = Parser->Stack.Top + 1;
        MoveTo(Parser, Begin);
        
        if (!TryParseAttributeList(Parser, &Result->Attributes, EndOfAttributes))
        {
            return false;
        }
    }
    
    MoveTo(Parser, End);
    
    if (CheckAt(Parser, ITokenType_Equals))
    {
        if (!ReceiveNextToken(Parser))
        {
            return false;
        }
        
        itoken_info BeginInitializer = Parser->At;
        while (!CheckAt(Parser, ITokenType_SemiColon))
        {
            if (CheckAt(Parser, ITokenType_End))
            {
                PrintLocation(Parser->At.Line, Parser->At.Column, Parser->At.Filename);
                printf("Unexpted EOF while parsing field initializer\n");
                return false;
            }
            
            if (!ReceiveNextToken(Parser))
            {
                return false;
            }
        }
        
        Result->HasInitializer = true;
        Result->InitializerText = CreateCText(&BeginInitializer, &Parser->At);
    }
    else
    {
        Result->HasInitializer = false;
    }
    
    // TryParseField doesn't move to the next token.
    return true;
}

static
char *BuildFilePath(import_file *File, char *CurrentDirectory)
{
    // + 1 for the added '/'
    size_t StringLength = strlen(CurrentDirectory) + 1 + File->Filename.Token.Length;
    
    // + 1 for the null terminator
    char *Buffer = (char *)malloc(StringLength + 1);
    sprintf(Buffer, "%s/%.*s", CurrentDirectory,
            (int)File->Filename.Token.Length, File->Filename.Token.Text);
    
    return Buffer;
}

static
bool StartParsingImport(inspect_parser *Parser, import_file *File)
{
    char *CurrentDirectory = Parser->Lexer->Directory;
    char *Filepath = BuildFilePath(File, CurrentDirectory);
    
    inspect_lexer *NewLexer = new inspect_lexer;
    if (!CreateLexer(Filepath, NewLexer))
    {
        PrintLocation(File->Filename.Line, File->Filename.Column,
                      Parser->Lexer->Filename);
        printf("Unable to open file \"%.*s\"\n",
               (int)File->Filename.Token.Length, File->Filename.Token.Text);
        return false;
    }
    
    Parser->LexerStorage.push_back(NewLexer);
    PushLexer(Parser, NewLexer);
    return true;
}

static
bool ReturnFromFile(inspect_parser *Parser)
{
    lex_state State;
    if (!PopLexer(&Parser->LexerStack, &State))
    {
        return false;
    }
    
    Parser->Lexer = State.Lexer;
    Parser->At = State.At;
    return true;
}

static
bool TryParseImport(inspect_parser *Parser, import_file *File)
{
    if (!CheckAt(Parser, ITokenType_Import))
    {
        return false;
    }
    
    if (!ExpectNext(Parser, ITokenType_String, "string"))
    {
        return false;
    }
    
    File->Filename = Parser->At;
    
    if (!ExpectNext(Parser, ITokenType_SemiColon, ";"))
    {
        return false;
    }
    
    return true;
}

static
bool TryParseDeclareType(inspect_parser *Parser, declared_type *Info)
{
    if (!CheckAt(Parser, ITokenType_DeclareType))
    {
        return false;
    }
    
    if (!CheckNext(Parser, ITokenType_Identifier))
    {
        printf("%s:%i:%i: Expected identifier after \"declare_type\"",
               Parser->Lexer->Filename, Parser->Lexer->Line, Parser->Lexer->Column);
        return false;
    }
    
    Info->TypeName = Parser->At;
    
    if (!CheckNext(Parser, ITokenType_Identifier))
    {
        printf("%s:%i:%i: Expected identifier after type name.",
               Parser->Lexer->Filename, Parser->Lexer->Line, Parser->Lexer->Column);
        return false;
    }
    
    Info->DescriptorName = Parser->At;
    
    if (!ExpectNext(Parser, ITokenType_SemiColon, ";"))
    {
        return false;
    }
    
    return true;
}

static
bool TryParseTypedArgumentReverse(inspect_parser *Parser,
                                  typed_argument_declaration *Result,
                                  int Barrier)
{
    if (!CheckAt(Parser, ITokenType_Identifier))
    {
        return false;
    }
    
    Result->Name = Parser->At;
    
    MoveBack(Parser);
    if (FailAtBarrierWithUnexpectedToken(Parser, Barrier))
    {
        return false;
    }
    
    if (!TryParseTypeReverse(Parser, &Result->Type, Barrier))
    {
        return false;
    }
    
    return true;
}

static
bool TryParseTypedArgumentListReverse(inspect_parser *Parser,
                                      typed_argument_list_declaration *Result,
                                      int Barrier)
{
    if (!CheckAt(Parser, ITokenType_RightParen))
    {
        return false;
    }
    
    MoveBack(Parser);
    if (FailAtBarrierWithUnexpectedToken(Parser, Barrier))
    {
        return false;
    }
    
    if (CheckAt(Parser, ITokenType_LeftParen))
    {
        MoveBack(Parser);
        return true;
    }
    
    do
    {
        typed_argument_declaration Argument;
        if (!TryParseTypedArgumentReverse(Parser, &Argument, Barrier))
        {
            return false;
        }
        
        Result->Arguments.push_back(Argument);
        
        if (CheckAt(Parser, ITokenType_Comma))
        {
            MoveBack(Parser);
            if (FailAtBarrierWithUnexpectedToken(Parser, Barrier))
            {
                return false;
            }
        }
        else if (CheckAt(Parser, ITokenType_LeftParen))
        {
            continue;
        }
        else
        {
            int AttributesEnd = Parser->Stack.Top + 1;
            
            do
            {
                MoveBack(Parser);
                if (FailAtBarrierWithUnexpectedToken(Parser, Barrier))
                {
                    return false;
                }
            }
            while (!CheckAt(Parser, ITokenType_Comma) &&
                   !CheckAt(Parser, ITokenType_LeftParen));
            
            int BeforeAttributes = Parser->Stack.Top;
            
            if (!ReceiveNextToken(Parser))
            {
                return false;
            }
            
            attribute_list *Attributes;
            if (!TryParseAttributeList(Parser, &Attributes, AttributesEnd))
            {
                return false;
            }
            
            MoveTo(Parser, BeforeAttributes);
        }
    }
    while (!CheckAt(Parser, ITokenType_LeftParen));
    
    MoveBack(Parser);
    return true;
}

static
bool TryParseArgumentList(inspect_parser *Parser, argument_list *Result)
{
    if (!CheckAt(Parser, ITokenType_LeftParen))
    {
        return false;
    }
    
    Result->ListBegin = Parser->At;
    
    if (CheckNext(Parser, ITokenType_RightParen))
    {
        return ReceiveNextToken(Parser);
    }
    
    while (!CheckAt(Parser, ITokenType_RightParen))
    {
        argument_item Parameter;
        Parameter.Named = false;
        
        itoken_info Begin = Parser->At;
        if (!ReceiveNextToken(Parser))
        {
            return false;
        }
        
        // Check if the parameter is named.
        if (Begin.Token.Type == ITokenType_Identifier)
        {
            if (CheckAt(Parser, ITokenType_Colon))
            {
                Parameter.Named = true;
                Parameter.Name = Begin;
                
                if (!ReceiveNextToken(Parser))
                {
                    return false;
                }
                
                Begin = Parser->At;
            }
        }
        
        while (!CheckAt(Parser, ITokenType_Comma) &&
               !CheckAt(Parser, ITokenType_RightParen))
        {
            if (CheckAt(Parser, ITokenType_End))
            {
                PrintLocation(Parser->At.Line, Parser->At.Column, Parser->At.Filename);
                printf("Unexpected EOF while parsing argument list\n");
                return false;
            }
            
            if (!ReceiveNextToken(Parser))
            {
                return false;
            }
        }
        
        Parameter.Value = CreateCText(&Begin, &Parser->At);
        Result->Arguments.push_back(Parameter);
        
        itoken_info Last = Parser->At;
        if (!ReceiveNextToken(Parser))
        {
            return false;
        }
        
        if (Last.Token.Type == ITokenType_RightParen)
        {
            break;
        }
    }
    
    return true;
}

static
bool TryParseArgumentListDeclaration(inspect_parser *Parser, argument_list_declaration *Result)
{
    if (!CheckAt(Parser, ITokenType_LeftParen))
    {
        return false;
    }
    
    if (CheckNext(Parser, ITokenType_RightParen))
    {
        return true;
    }
    
    for (;;)
    {
        if (!ExpectAt(Parser, ITokenType_Identifier, "Identifier"))
        {
            return false;
        }
        
        Result->Names.push_back(Parser->At);
        
        if (CheckNext(Parser, ITokenType_RightParen))
        {
            break;
        }
        else if (CheckAt(Parser, ITokenType_Comma))
        {
            ReceiveNextToken(Parser);
        }
        else if (CheckAt(Parser, ITokenType_End))
        {
            PrintLocation(Parser->At.Line, Parser->At.Column, Parser->At.Filename);
            printf("Unexpected EOF while parsing argument list\n");
            return false;
        }
        else
        {
            PrintLocation(Parser->At.Line, Parser->At.Column, Parser->At.Filename);
            printf("Unexpected \"%.*s\" while parsing argument list\n",
                   (int)Parser->At.Token.Length, Parser->At.Token.Text);
            return false;
        }
    }
    
    return true;
}

static
bool TryParseAttributeInstance(inspect_parser *Parser,
                               attribute_instance *Result,
                               bool *ParsedAttribute)
{
    *ParsedAttribute = false;
    if (!CheckAt(Parser, ITokenType_LeftSquare))
    {
        if (!CheckAt(Parser, ITokenType_Identifier))
        {
            return true;
        }
        
        Result->InfoHandle = INVALID_ATTRIBUTE_HANDLE;
        Result->IdentifierToken = Parser->At;
        Result->Aliased = true;
        Result->Alias = 0;
        *ParsedAttribute = true;
        return true;
    }
    
    if (!ExpectNext(Parser, ITokenType_Identifier, "Identifier"))
    {
        return false;
    }
    
    itoken_info Identifier = Parser->At;
    
    if (!ReceiveNextToken(Parser))
    {
        return false;
    }
    
    Result->InfoHandle = INVALID_ATTRIBUTE_HANDLE;
    Result->IdentifierToken = Identifier;
    Result->Aliased = false;
    Result->Alias = 0;
    
    if (!TryParseArgumentList(Parser, &Result->Arguments))
    {
        return false;
    }
    
    if (!ExpectAt(Parser, ITokenType_RightSquare, "]"))
    {
        return false;
    }
    
    *ParsedAttribute = true;
    return true;
}

static
bool TryParseAliasAttribute(inspect_parser *Parser, attribute_alias *Result)
{
    if (!CheckAt(Parser, ITokenType_AliasAttribute))
    {
        return false;
    }
    itoken_info Start = Parser->At;
    
    if (!ExpectNext(Parser, ITokenType_Identifier, "Identifier"))
    {
        return false;
    }
    
    Result->Alias = Parser->At;
    
    if (!ReceiveNextToken(Parser))
    {
        return false;
    }
    
    bool Parsed;
    if (!TryParseAttributeInstance(Parser, &Result->Value, &Parsed) && Parsed)
    {
        PrintLocation(Start.Line,
                      Start.Column,
                      Start.Filename);
        printf("Expected attribute\n");
        return false;
    }
    
    return true;
}

static
bool TryParseAttributeList(inspect_parser *Parser,
                           attribute_list **Result,
                           int Until)
{
    // NOTE(Brian): Unlike most of the TryParse* functions, this function
    // does not return false if it does not produce anything.
    *Result = 0;
    
    while (Parser->Stack.Top < Until)
    {
        attribute_instance NewAttribute;
        bool ParsedAttribute;
        if (!TryParseAttributeInstance(Parser, &NewAttribute, &ParsedAttribute))
        {
            if (*Result)
            {
                attribute_instance &First = (*Result)->Attributes.front();
                PrintLocation(First.IdentifierToken.Line,
                              First.IdentifierToken.Column,
                              First.IdentifierToken.Filename);
                printf("Failed to parse attribute list starting at \"%.*s\"\n",
                       (int)First.IdentifierToken.Token.Length,
                       First.IdentifierToken.Token.Text);
            }
            
            return false;
        }
        
        if (!ParsedAttribute)
        {
            break;
        }
        
        if (*Result == nullptr)
        {
            *Result = new attribute_list;
        }
        
        (*Result)->Attributes.push_back(NewAttribute);
        
        if (!ReceiveNextToken(Parser))
        {
            return false;
        }
    }
    
    if (*Result)
    {
        Parser->UnresolvedAttributeLists.push_back(*Result);
    }
    
    return true;
}

static
bool TryParseDeclareAttribute(inspect_parser *Parser, attribute_declaration *Result)
{
    if (!CheckAt(Parser, ITokenType_DeclareAttribute))
    {
        return false;
    }
    
    if (!ExpectNext(Parser, ITokenType_Identifier, "Identifier"))
    {
        return false;
    }
    
    itoken_info Identifier = Parser->At;
    Result->Name = Identifier;
    
    if (!ReceiveNextToken(Parser))
    {
        return false;
    }
    
    if (!TryParseArgumentListDeclaration(Parser, &Result->ArgumentList))
    {
        return false;
    }
    
    return true;
}

static
bool TryParseStruct(inspect_parser *Parser, defined_struct *Result)
{
    if (CheckAt(Parser, ITokenType_Struct))
    {
        if (!CheckNext(Parser, ITokenType_Identifier))
        {
            printf("%s:%i:%i: Expected identifier after \"struct\"",
                   Parser->Lexer->Filename, Parser->Lexer->Line, Parser->Lexer->Column);
            return false;
        }
        Result->Identifier = Parser->At;
        
        if (!ExpectNext(Parser, ITokenType_LeftCurly, "{"))
        {
            return false;
        }
        
        for (;;)
        {
            if (!ReceiveNextToken(Parser))
            {
                return false;
            }
            
            if (CheckAt(Parser, ITokenType_RightCurly))
            {
                break;
            }
            
            field Field;
            if (!TryParseField(Parser, &Field))
            {
                return false;
            }
            
            Result->Fields.push_back(Field);
        }
        
        if (!ExpectNext(Parser, ITokenType_SemiColon, ";"))
        {
            return false;
        }
        
        return true;
    }
    
    return false;
}

static inline
bool CompareTokenNames(inspect_token *First, inspect_token *Second)
{
    if (First->Length != Second->Length)
    {
        return false;
    }
    
    for (size_t I = 0; I < First->Length; ++I)
    {
        if (First->Text[I] != Second->Text[I])
        {
            return false;
        }
    }
    
    return true;
}

static
bool ResolveArguments(argument_list_declaration *Signature,
                      argument_list *List,
                      inspect_dict *Parent)
{
    if (Signature->Names.size() != List->Arguments.size())
    {
        PrintLocation(List->ListBegin.Line,
                      List->ListBegin.Column,
                      List->ListBegin.Filename);
        printf("Expected %zu arguments, found %zu.\n",
               Signature->Names.size(),
               List->Arguments.size());
        return false;
    }
    
    for(size_t I = 0; I < List->Arguments.size(); ++I)
    {
        itoken_info &SignatureName = Signature->Names[I];
        argument_item &Argument = List->Arguments[I];
        
        if (Argument.Named)
        {
            if (!CompareTokenNames(&Argument.Name.Token, &SignatureName.Token))
            {
                PrintLocation(Argument.Name.Line,
                              Argument.Name.Column,
                              Argument.Name.Filename);
                printf("Explicit argument name doesn't match signature, found \"%.*s\" expected \"%.*s\"\n",
                       (int)Argument.Name.Token.Length,
                       Argument.Name.Token.Text,
                       (int)SignatureName.Token.Length,
                       SignatureName.Token.Text);
                
                return false;
            }
        }
        
        Insert(Parent, &SignatureName.Token, NewStringItem(&Argument.Value));
    }
    
    return true;
}

static
bool BuildAttributeData(inspect_parser *Parser, attribute_list *List)
{
    List->AttributeData.Parent = nullptr;
    
    for (attribute_instance &Instance : List->Attributes)
    {
        attribute_declaration *Declaration;
        attribute_instance *ActualAttribute;
        if (!Instance.Aliased)
        {
            Declaration = &Parser->AttributeInformation[(size_t)Instance.InfoHandle];
            ActualAttribute = &Instance;
        }
        else
        {
            ActualAttribute = Instance.Alias;
            Declaration = &Parser->AttributeInformation[(size_t)ActualAttribute->InfoHandle];
        }
        
        inspect_data_item DictItem = NewDictItem();
        inspect_dict &Dict = *DictItem.Dict;
        
        if (!ResolveArguments(&Declaration->ArgumentList, &ActualAttribute->Arguments, &Dict))
        {
            FreeDataItem(&DictItem);
            return false;
        }
        
        Insert(&List->AttributeData, &ActualAttribute->IdentifierToken.Token, &DictItem);
    }
    
    return true;
}

static
bool ResolveAttribute(inspect_parser *Parser, attribute_instance *Instance)
{
    for (attribute_handle Handle = 0;
         Handle < (int)Parser->AttributeInformation.size();
         ++Handle)
    {
        attribute_declaration &Declaration = Parser->AttributeInformation[(uint64)Handle];
        if (CompareTokenNames(&Declaration.Name.Token,
                              &Instance->IdentifierToken.Token))
        {
            Instance->InfoHandle = Handle;
        }
    }
    
    if (Instance->InfoHandle == INVALID_ATTRIBUTE_HANDLE)
    {
        PrintLocation(Instance->IdentifierToken.Line,
                      Instance->IdentifierToken.Column,
                      Instance->IdentifierToken.Filename);
        printf("Unrecognized Attribute \"%.*s\"\n",
               (int)Instance->IdentifierToken.Token.Length,
               Instance->IdentifierToken.Token.Text);
        return false;
    }
    
    return true;
}

static
bool LinkAttribute(inspect_parser *Parser, attribute_instance *Instance)
{
    for(attribute_alias &Alias : Parser->AttributeAliases)
    {
        if (CompareTokenNames(&Instance->IdentifierToken.Token,
                              &Alias.Alias.Token))
        {
            Instance->Alias = &Alias.Value;
            return true;
        }
    }
    
    PrintLocation(Instance->IdentifierToken.Line,
                  Instance->IdentifierToken.Column,
                  Instance->IdentifierToken.Filename);
    printf("Could not resolve attribute alias \"%.*s\"\n",
           (int)Instance->IdentifierToken.Token.Length,
           Instance->IdentifierToken.Token.Text);
    return false;
}

static
bool ResolveAttributes(inspect_parser *Parser)
{
    for (attribute_alias &Alias : Parser->AttributeAliases)
    {
        if (!ResolveAttribute(Parser, &Alias.Value))
        {
            return false;
        }
    }
    
    for (attribute_list *List : Parser->UnresolvedAttributeLists)
    {
        for (attribute_instance &Instance : List->Attributes)
        {
            if (Instance.Aliased)
            {
                if (!LinkAttribute(Parser, &Instance))
                {
                    return false;
                }
            }
            
            else
            {
                if (!ResolveAttribute(Parser, &Instance))
                {
                    return false;
                }
            }
        }
        
        if (!BuildAttributeData(Parser, List))
        {
            return false;
        }
    }
    
    return true;
}

static
bool ResolveType(inspect_parser *Parser, inspect_data_item Unresolved)
{
    inspect_dict *UnresolvedTypeDict = Unresolved.Dict;
    inspect_list &TypeList = *Parser->TypeInfoList.List;
    
    for(;;)
    {
        // We only need to resolve the innermost type of
        // pointer or references.
        
        bool IsPointer = UnresolvedTypeDict->Lookup.at("IsPointer").Bool;
        bool IsReference = UnresolvedTypeDict->Lookup.at("IsReference").Bool;
        
        if (IsPointer || IsReference)
        {
            // The first item in the type info list is the PTR type info.
            Insert(UnresolvedTypeDict, "Info", CreateReference(TypeList[0].Dict));
            Unresolved = UnresolvedTypeDict->Lookup.at("InnerType");
            UnresolvedTypeDict = Unresolved.Dict;
        }
        else
        {
            break;
        }
    }
    
    const char *TypeName = UnresolvedTypeDict->Lookup.at("Name").String;
    bool Found = false;
    for (inspect_data_item &TypeItem : TypeList)
    {
        const char *DeclaredTypeName = TypeItem.Dict->Lookup.at("Name").String;
        if (strcmp(DeclaredTypeName, TypeName) == 0)
        {
            Found = true;
            
            Insert(UnresolvedTypeDict, "Info", CreateReference(TypeItem.Dict));
            break;
        }
    }
    
    if (!Found)
    {
        PrintLocation(Unresolved.OptionalSourceToken.Line,
                      Unresolved.OptionalSourceToken.Column,
                      Unresolved.OptionalSourceToken.Filename);
        printf("Unrecognized type \"%s\"\n",
               TypeName);
        return false;
    }

    inspect_list *Args = UnresolvedTypeDict->Lookup.at("Args").List;
    for (inspect_data_item &Item : *Args)
    {
        if (!ResolveType(Parser, Item))
        {
            return false;
        }
    }
    
    return true;
}

static
bool ResolveTypes(inspect_parser *Parser)
{
    inspect_list &StructList = *Parser->StructList.List;
    
    for (inspect_data_item &StructItem : StructList)
    {
        inspect_list &FieldList = *StructItem.Dict->Lookup.at("Fields").List;
        
        for (inspect_data_item &FieldItem : FieldList)
        {
            inspect_dict *FieldDict = FieldItem.Dict;
            inspect_data_item TypeItem = FieldDict->Lookup.at("Type");
            if (!ResolveType(Parser, TypeItem))
            {
                return false;
            }
            
            bool IsMethod = FieldDict->Lookup.at("IsMethod").Bool;
            if (IsMethod)
            {
                inspect_list *ArgumentList = FieldDict->Lookup.at("MethodArguments").List;
                for (inspect_data_item &ArgumentItem : *ArgumentList)
                {
                    inspect_dict *ArgumentDict = ArgumentItem.Dict;
                    inspect_data_item ArgumentType = ArgumentDict->Lookup.at("Type");
                    if (!ResolveType(Parser, ArgumentType))
                    {
                        return false;
                    }
                }
            }
        }
    }
    
    return true;
}

static inline
void InitializeTypeInfoList(inspect_list *List)
{
    // Later we get the PTR type info from accessing the first
    // element in the list.
    // Its important that this is the first thing inserted into the list!
    List->push_back(CreateTypeInfoItem("Pointer", "TD_PTR", nullptr));
}

bool CreateParser(const char *Filename, inspect_parser *Parser)
{
    inspect_lexer *NewLexer = new inspect_lexer;
    Parser->LexerStorage.push_back(NewLexer);
    if(!CreateLexer(Filename, NewLexer))
    {
        return false;
    }
    
    CreateTokenStack(&Parser->Stack);
    
    CreateLexerStack(&Parser->LexerStack);
    Parser->Lexer = NewLexer;
    
    Parser->StructList = NewListItem();
    Parser->TypeInfoList = NewListItem();
    InitializeTypeInfoList(Parser->TypeInfoList.List);
    return true;
}

void FreeParser(inspect_parser *Parser)
{
    for(inspect_lexer *Lexer : Parser->LexerStorage)
    {
        FreeLexer(Lexer);
        delete Lexer;
    }
}

static inline
bool ShouldGenerateStructs(inspect_parser *Parser)
{
    // We only want to write out struct information
    // if we are not parsing import files.
    return Parser->LexerStack.Top == -1;
}

static void
FreeType(type *Type)
{
    if (Type->IsPointer || Type->IsReference)
    {
        FreeType(Type->InnerType);
        delete Type->InnerType;
    }
}

static void
FreeField(field *Field)
{
    // We don't need to free the attribute list,
    // it doesn't belong to the field.
    
    FreeType(&Field->Type);
}

static void
FreeStruct(defined_struct *Struct)
{
    for(field &Field : Struct->Fields)
    {
        FreeField(&Field);
    }
}

bool ParseInspect(inspect_parser *Parser, inspect_data *Data)
{
    for(;;)
    {
        if (CheckNext(Parser, ITokenType_End))
        {
            if (!ReturnFromFile(Parser))
            {
                break;
            }
            else
            {
                // edge case: we could have been at the end of the last file.
                if (CheckAt(Parser, ITokenType_End))
                {
                    break;
                }
            }
        }
        
        attribute_list *PendingAttributes;
        if (!TryParseAttributeList(Parser, &PendingAttributes))
        {
            return false;
        }
        
        // NOTE(Brian): TryParseAttributeList is the one special function that
        // ends on the next token, so we have to check for the end of file here.
        if (CheckAt(Parser, ITokenType_End))
        {
            break;
        }
        
        defined_struct Struct;
        if (TryParseStruct(Parser, &Struct))
        {
            inspect_data_item StructType = CreateTypeInfoItem(&Struct, PendingAttributes);
            
            if (ShouldGenerateStructs(Parser))
            {
                Parser->StructList.List->push_back(CreateStructItem(&Struct, StructType.Dict, PendingAttributes));
            }
            
            Parser->TypeInfoList.List->push_back(StructType);
            PendingAttributes = 0;
            FreeStruct(&Struct);
            
            continue;
        }
        
        declared_type TypeInfo;
        if (TryParseDeclareType(Parser, &TypeInfo))
        {
            Parser->TypeInfoList.List->push_back(CreateTypeInfoItem(&TypeInfo, PendingAttributes));
            PendingAttributes = 0;
        }
        
        attribute_alias Alias;
        if (TryParseAliasAttribute(Parser, &Alias))
        {
            Parser->AttributeAliases.push_back(Alias);
        }
        
        attribute_declaration AttributeInfo;
        if (TryParseDeclareAttribute(Parser, &AttributeInfo))
        {
            Parser->AttributeInformation.push_back(AttributeInfo);
        }
        
        import_file ImportFile;
        if (TryParseImport(Parser, &ImportFile))
        {
            if (!StartParsingImport(Parser, &ImportFile))
            {
                return false;
            }
        }
        
        if (PendingAttributes != nullptr)
        {
            attribute_instance &First = PendingAttributes->Attributes.front();
            PrintLocation(First.IdentifierToken.Line,
                          First.IdentifierToken.Column,
                          First.IdentifierToken.Filename);
            printf("Attribute list cannot be defined here. First attribute \"%.*s\"\n",
                   (int)First.IdentifierToken.Token.Length,
                   First.IdentifierToken.Token.Text);
            PendingAttributes = 0;
            return false;
        }
    }
    
    if (!ResolveTypes(Parser))
    {
        return false;
    }
    
    if (!ResolveAttributes(Parser))
    {
        return false;
    }
    
    Insert(Data->GlobalScope.Dict, "Structs", &Parser->StructList);
    Insert(Data->GlobalScope.Dict, "Types", &Parser->TypeInfoList);
    return true;
}
