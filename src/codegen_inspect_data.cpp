
#include <stdlib.h>
#include <string.h>
#include <unordered_map>
#include <assert.h>
#include "codegen_inspect_data.h"
#include "compiler_utils.h"

void FreeDataItem(inspect_data_item *Item)
{
    if (Item->IsReference)
    {
        return;
    }
    
    if (Item->Type == Type_Dict)
    {
        delete Item->Dict;
    }
    else if (Item->Type == Type_List)
    {
        delete Item->List;
    }
    else if (Item->Type == Type_String)
    {
        free(Item->String);
    }
    else if (Item->Type == Type_Procedure)
    {
        delete Item->Procedure;
    }
    
    if (Item->Attributes)
    {
        delete Item->Attributes;
    }
}

void FreeInspectDict(inspect_dict *Dict)
{
    for(auto Iterator = Dict->Lookup.begin();
        Iterator != Dict->Lookup.end();
        Iterator++)
    {
        FreeDataItem(&Iterator->second);
    }
}

bool Lookup(inspect_dict *Dict, std::string Key, inspect_data_item *Value)
{
    auto Result = Dict->Lookup.find(Key);
    if (Result == Dict->Lookup.end())
    {
        if (Dict->Parent)
        {
            return Lookup(Dict->Parent, Key, Value);
        }
        
        return false;
    }
    
    *Value = Result->second;
    return true;
}

/*******************************************/
// Operation interfaces

inspect_data_item FailIfCalled(inspect_data_item *Left, inspect_data_item *Right)
{
    REF(Left);
    REF(Right);
    assert(false);
    return {};
}

inspect_data_item FailIfCalled(inspect_data_item *Item)
{
    REF(Item);
    assert(false);
    return {};
}

bool NoValidOperation(inspect_item_operator Op)
{
    REF(Op);
    return false;
}

bool NoValidCast(inspect_item_type Type, inspect_data_item *Item, inspect_data_item *Result)
{
    REF(Type);
    REF(Item);
    REF(Result);
    return false;
}

bool BoolCanExecuteOperation(inspect_item_operator Op)
{
    return Op == Equality_Op ||
        Op == Inequality_Op ||
        Op == Not_Op ||
        Op == BooleanOr_Op ||
        Op == BooleanAnd_Op;
}

bool IntCanExecuteOperation(inspect_item_operator Op)
{
    return Op == Addition_Op ||
        Op == Subtraction_Op ||
        Op == Multiplication_Op ||
        Op == Division_Op ||
        Op == Equality_Op ||
        Op == Inequality_Op ||
        Op == GreaterThan_Op ||
        Op == LessThan_Op ||
        Op == Increment_Op ||
        Op == Decrement_Op ||
        Op == Negative_Op;
}

inspect_data_item IntAddition(inspect_data_item *Left, inspect_data_item *Right)
{
    return NewIntItem(Left->Int + Right->Int);
}

inspect_data_item IntSubtraction(inspect_data_item *Left, inspect_data_item *Right)
{
    return NewIntItem(Left->Int - Right->Int);
}

inspect_data_item IntMultiplication(inspect_data_item *Left, inspect_data_item *Right)
{
    return NewIntItem(Left->Int * Right->Int);
}

inspect_data_item IntDivision(inspect_data_item *Left, inspect_data_item *Right)
{
    return NewIntItem(Left->Int / Right->Int);
}

inspect_data_item IntEquality(inspect_data_item *Left, inspect_data_item *Right)
{
    return NewBoolItem(Left->Int == Right->Int);
}

inspect_data_item IntInequality(inspect_data_item *Left, inspect_data_item *Right)
{
    return NewBoolItem(Left->Int != Right->Int);
}

inspect_data_item IntGreaterThan(inspect_data_item *Left, inspect_data_item *Right)
{
    return NewBoolItem(Left->Int > Right->Int);
}

inspect_data_item IntLessThan(inspect_data_item *Left, inspect_data_item *Right)
{
    return NewBoolItem(Left->Int < Right->Int);
}

inspect_data_item IntNegative(inspect_data_item *Item)
{
    return NewIntItem(-Item->Int);
}

inspect_data_item IntIncrement(inspect_data_item *Item)
{
    return NewIntItem(Item->Int + 1);
}

inspect_data_item IntDecrement(inspect_data_item *Item)
{
    return NewIntItem(Item->Int + 1);
}

inspect_data_item BoolEquality(inspect_data_item *Left, inspect_data_item *Right)
{
    return NewBoolItem(Left->Bool == Right->Bool);
}

inspect_data_item BoolInequality(inspect_data_item *Left, inspect_data_item *Right)
{
    return NewBoolItem(Left->Bool != Right->Bool);
}

inspect_data_item BoolOr(inspect_data_item *Left, inspect_data_item *Right)
{
    return NewBoolItem(Left->Bool || Right->Bool);
}

inspect_data_item BoolAnd(inspect_data_item *Left, inspect_data_item *Right)
{
    return NewBoolItem(Left->Bool && Right->Bool);
}

inspect_data_item BoolNot(inspect_data_item *Item)
{
    return NewBoolItem(!Item->Bool);
}

bool StringCanExecute(inspect_item_operator Op)
{
    return Op == Equality_Op ||
        Op == Inequality_Op;
}

inspect_data_item StringEquality(inspect_data_item *Left, inspect_data_item *Right)
{
    return NewBoolItem(strcmp(Left->String, Right->String) == 0);
}

inspect_data_item StringInEquality(inspect_data_item *Left, inspect_data_item *Right)
{
    return NewBoolItem(strcmp(Left->String, Right->String) != 0);
}

inspect_data_operation_interface InspectDataInterfaces[Num_Inspect_Item_Types] =
{
    
    /*******************************************/
    // String
    
    InspectDataInterfaces[Type_String] =
    {
        StringCanExecute,
        NoValidCast,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        StringEquality,
        StringInEquality,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
    },
    
    /*******************************************/
    // Int
    
    InspectDataInterfaces[Type_Int] =
    {
        IntCanExecuteOperation,
        NoValidCast,
        IntAddition,
        IntSubtraction,
        IntMultiplication,
        IntDivision,
        IntEquality,
        IntInequality,
        IntGreaterThan,
        IntLessThan,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        IntNegative,
        IntIncrement,
        IntDecrement
    },
    
    /*******************************************/
    // Bool
    
    InspectDataInterfaces[Type_Bool] =
    {
        BoolCanExecuteOperation,
        NoValidCast,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        BoolEquality,
        BoolInequality,
        FailIfCalled,
        FailIfCalled,
        BoolOr,
        BoolAnd,
        BoolNot,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
    },
    
    /*******************************************/
    // Void
    
    InspectDataInterfaces[Type_Void] =
    {
        NoValidOperation,
        NoValidCast,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
    },
    
    /*******************************************/
    // Dict
    
    InspectDataInterfaces[Type_Dict] =
    {
        NoValidOperation,
        NoValidCast,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
    },
    
    /*******************************************/
    // List
    
    InspectDataInterfaces[Type_List] =
    {
        NoValidOperation,
        NoValidCast,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
    },
    
    /*******************************************/
    // Procedure
    
    InspectDataInterfaces[Type_Procedure] =
    {
        NoValidOperation,
        NoValidCast,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
        FailIfCalled,
    },
};
