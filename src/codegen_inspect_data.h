#pragma once
#include <unordered_map>
#include "codegen_lex_inspect.h"
#include "codegen_lex_write.h"
#include "numeric_types.h"

enum inspect_item_type
{
    Type_String,
    Type_Int,
    Type_Bool,
    Type_Void,
    Type_Dict,
    Type_List,
    Type_Procedure,
    Num_Inspect_Item_Types,
};

inline
const char *InspectItemTypeToString(inspect_item_type Type)
{
    switch(Type)
    {
        case Type_String: return "String";
        case Type_Int: return "Int";
        case Type_Bool: return "Bool";
        case Type_Void: return "Void";
        case Type_Dict: return "Dict";
        case Type_List: return "List";
        case Type_Procedure: return "Procedure";
        case Num_Inspect_Item_Types: return "Num_Inspect_Item_Types";
    }
    
    return "Unknown";
}

struct itoken_info
{
    inspect_token Token;
    int Line;
    int Column;
    char *Filename;
};

struct inspect_data_item;
typedef std::vector<inspect_data_item> inspect_list;

struct inspect_dict
{
    inspect_dict *Parent;
    std::unordered_map<std::string, inspect_data_item> Lookup;
};

struct tab_state
{
    int32 TabsToAdd;
    int32 TabsToRemove;
};

struct inspect_procedure
{
    std::vector<wtoken_info> Args;
    inspect_dict *ParentScope;
    int BodyLocation;
    tab_state TabState;
};

struct inspect_ctext
{
    size_t Length;
    char *Begin;
};

struct argument_item
{
    bool Named;
    itoken_info Name;
    inspect_ctext Value;
};

struct argument_list
{
    itoken_info ListBegin;
    std::vector<argument_item> Arguments;
};

typedef int32 attribute_handle;
#define INVALID_ATTRIBUTE_HANDLE -1

struct attribute_instance
{
    attribute_handle InfoHandle;
    itoken_info IdentifierToken;
    argument_list Arguments;
    bool Aliased;
    attribute_instance *Alias;
};

struct attribute_list
{
    std::vector<attribute_instance> Attributes;
    
    inspect_dict AttributeData;
};

struct inspect_data_item
{
    inspect_data_item()
    {
        UID = NextUID++;
        Attributes = 0;
    }
    
    inspect_item_type Type;
    union
    {
        char *String;
        int Int;
        bool Bool;
        itoken_info TokenInfo;
        inspect_ctext CText;
        inspect_dict *Dict;
        inspect_list *List;
        inspect_procedure *Procedure;
    };
    
    inspect_dict *Owner = 0;
    bool IsReference = false;
    
    uint64 UID;
    inline static uint64 NextUID = 0;
    
    attribute_list *Attributes;
    
    itoken_info OptionalSourceToken;
};

enum inspect_item_operator
{
    Addition_Op,
    Subtraction_Op,
    Multiplication_Op,
    Division_Op,
    Equality_Op,
    Inequality_Op,
    GreaterThan_Op,
    LessThan_Op,
    BooleanOr_Op,
    BooleanAnd_Op,
    Not_Op,
    Negative_Op,
    Increment_Op,
    Decrement_Op,
};

typedef inspect_data_item (*binary_op)(inspect_data_item *, inspect_data_item *);
typedef inspect_data_item (*unary_op)(inspect_data_item *);
typedef bool (*query_execute)(inspect_item_operator);
typedef bool (*cast_op)(inspect_item_type, inspect_data_item *, inspect_data_item *);

// The dot operator and assignment are handled in the parser.
struct inspect_data_operation_interface
{
    query_execute CanExecuteOperation;
    cast_op Cast;
    binary_op Add;
    binary_op Subtract;
    binary_op Multiply;
    binary_op Divide;
    binary_op Equals;
    binary_op DoesNotEquals;
    binary_op GreaterThan;
    binary_op LessThan;
    binary_op BooleanOr;
    binary_op BooleanAnd;
    unary_op Not;
    unary_op Negate;
    unary_op Increment;
    unary_op Decrement;
};

extern inspect_data_operation_interface InspectDataInterfaces[Num_Inspect_Item_Types];

inline
inspect_data_operation_interface *GetInterface(inspect_item_type Type)
{
    return &InspectDataInterfaces[Type];
}

struct inspect_data
{
    inspect_data_item GlobalScope;
};

inline
inspect_dict *NewDict()
{
    inspect_dict *Result = new inspect_dict;
    Result->Parent = 0;
    return Result;
}

inline
inspect_data_item NewDictItem()
{
    inspect_data_item Item;
    Item.Type = Type_Dict;
    Item.Dict = NewDict();
    return Item;
}

inline
inspect_data_item NewListItem()
{
    inspect_data_item Item;
    Item.Type = Type_List;
    Item.List = new inspect_list;
    return Item;
}

inline
inspect_data_item NewProcedureItem()
{
    inspect_data_item Item;
    Item.Type = Type_Procedure;
    Item.Procedure = new inspect_procedure;
    return Item;
}

inline
inspect_data_item NewStringItem(const char *String)
{
    inspect_data_item Item;
    Item.Type = Type_String;
    Item.String = strdup(String);
    return Item;
}

inline inspect_data_item NewStringItem(itoken_info *Token)
{
    inspect_data_item Item;
    Item.Type = Type_String;
    Item.String = (char *)malloc(Token->Token.Length + 1);
    memcpy(Item.String, Token->Token.Text, Token->Token.Length);
    Item.String[Token->Token.Length] = '\0';
    return Item;
}

inline
inspect_data_item NewStringItem(inspect_ctext *Text)
{
    inspect_data_item Item;
    Item.Type = Type_String;
    Item.String = (char *)malloc(Text->Length + 1);
    memcpy(Item.String, Text->Begin, Text->Length);
    Item.String[Text->Length] = '\0';
    return Item;
}

inline
inspect_data_item ReceiveStringItem(char *String)
{
    inspect_data_item Item;
    Item.Type = Type_String;
    Item.String = String;
    return Item;
}

inline
inspect_data_item NewIntItem(int Int)
{
    inspect_data_item Item;
    Item.Type = Type_Int;
    Item.Int = Int;
    return Item;
}

inline
inspect_data_item NewBoolItem(bool Bool)
{
    inspect_data_item Item;
    Item.Type = Type_Bool;
    Item.Bool = Bool;
    return Item;
}

inline
inspect_data_item NewVoidItem()
{
    inspect_data_item Item;
    Item.Type = Type_Void;
    return Item;
}

void FreeDataItem(inspect_data_item *Item);

inline
std::string StringFromToken(write_token *Token)
{
    return std::string(Token->Text, Token->Length);
}

inline
std::string StringFromToken(inspect_token *Token)
{
    return std::string(Token->Text, Token->Length);
}

// NOTE(Brian): For insert, we set the item's owner variable. To prevent mistakes,
// we don't allow passing the item by copy, but an r-value is fine.
inline
void Insert(inspect_dict *Dict, const char *Key, inspect_data_item *Value)
{
    Value->Owner = Dict;
    Dict->Lookup[Key] = *Value;
}

inline
void Insert(inspect_dict *Dict, const char *Key, inspect_data_item &&Value)
{
    Value.Owner = Dict;
    Dict->Lookup[Key] = Value;
}

inline
void Insert(inspect_dict *Dict, inspect_token *TokenKey, inspect_data_item &&Value)
{
    Value.Owner = Dict;
    Dict->Lookup[StringFromToken(TokenKey)] = Value;
}

inline
void Insert(inspect_dict *Dict, inspect_token *TokenKey, inspect_data_item *Value)
{
    Value->Owner = Dict;
    Dict->Lookup[StringFromToken(TokenKey)] = *Value;
}

inline
void Insert(inspect_dict *Dict, write_token *TokenKey, inspect_data_item *Value)
{
    Value->Owner = Dict;
    Dict->Lookup[StringFromToken(TokenKey)] = *Value;
}

inline
void FreeIfExists(inspect_dict *Dict, const char *Key)
{
    auto It = Dict->Lookup.find(Key);
    if (It != Dict->Lookup.end())
    {
        FreeDataItem(&It->second);
        Dict->Lookup.erase(It);
    }
}

#if 0
inline
void Remove(inspect_dict *Dict, write_token *TokenKey)
{
    auto It = Dict->Lookup.find(StringFromToken(TokenKey));
    FreeDataItem(It->second());
    Dict->Lookup.erase(It);
}
#endif

bool Lookup(inspect_dict *Dict, std::string Key, inspect_data_item *Value);

inline
bool Lookup(inspect_dict *Dict, write_token *TokenIdentifier, inspect_data_item *Value)
{
    return Lookup(Dict, StringFromToken(TokenIdentifier), Value);
}

inline
inspect_data_item CreateCopyOrReference(inspect_data_item *Item)
{
    inspect_data_item Result = *Item;
    
    if (Item->Type == Type_Dict ||
        Item->Type == Type_List ||
        Item->Type == Type_String)
    {
        Result.IsReference = true;
    }
    
    return Result;
}

inline
inspect_data_item CreateReference(inspect_dict *Dict)
{
    inspect_data_item Result;
    Result.Type = Type_Dict;
    Result.Dict = Dict;
    Result.IsReference = true;
    return Result;
}
