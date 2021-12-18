#include "codegen_parse_write.h"
#include "codegen_parse_inspect.h"
#include "codegen_lex_base.h"
#include "platform.h"

#define CODEGEN_SUCCESS 0
#define CODEGEN_FAILURE 1

struct command_options
{
    char *InputFile;
    char *OutputDirectory;
    bool DoNotRun;
    bool UseDebugFiles;
};

static inline
void PrintUsage()
{
    printf("Usage: codegen inputfile -O outputdir\n");
}

static inline
bool CreateCommandOptions(int argc, char **argv, command_options *Options)
{
    Options->InputFile = 0;
    Options->OutputDirectory = 0;
    Options->DoNotRun = false;
    Options->UseDebugFiles = false;
    
    if (argc == 1)
    {
        printf("Invalid command line: No input file specified.\n");
        PrintUsage();
        return false;
    }
    
    for (int I = 1; I < argc; ++I)
    {
        if (strcmp(argv[I], "-O") == 0 ||
            strcmp(argv[I], "/O") == 0)
        {
            if (Options->OutputDirectory)
            {
                printf("Invalid command line: Multiple uses of \"%s\"\n", argv[I]);
                PrintUsage();
                return false;
            }
            
            int Next = I + 1;
            if (Next >= argc)
            {
                printf("Invalid command line: Expected file name after \"%s\"\n", argv[I]);
                PrintUsage();
                return false;
            }
            
            if (!PLATFORM_IS_DIRECTORY(argv[Next]))
            {
                printf("Invalid command line: \"%s\" is not a directory.\n", argv[I]);
                PrintUsage();
                return false;
            }
            
            Options->OutputDirectory = argv[Next];
            I = Next;
        }
        else if (strcmp(argv[I], "-?") == 0 ||
                 strcmp(argv[I], "/?") == 0)
        {
            Options->DoNotRun = true;
            PrintUsage();
        }
        else if (strcmp(argv[I], "-D") == 0 ||
                 strcmp(argv[I], "/D") == 0)
        {
            Options->UseDebugFiles = true;
        }
        else if(argv[I][0] == '/' ||
                argv[I][0] == '-')
        {
            printf("Invalid command line: Unknown switch \"%s\"\n", argv[I]);
            PrintUsage();
            return false;
        }
        else
        {
            if (Options->InputFile)
            {
                printf("Invalid command line: Input file specified multiple times (\"%s\" and \"%s\").\n",
                       Options->InputFile,
                       argv[I]);
                PrintUsage();
                return false;
            }
            
            Options->InputFile = argv[I];
        }
    }
    
    if (Options->UseDebugFiles)
    {
        if (Options->InputFile || Options->OutputDirectory)
        {
            printf("Invalid command line: No input file or output directory can be specified when using the \"/D\" switch.\n");
            return false;
        }
        
        return true;
    }
    
    if (!Options->InputFile)
    {
        printf("Invalid command line: Input file required.\n");
        PrintUsage();
        return false;
    }
    
    if (!Options->OutputDirectory)
    {
        printf("Invalid command line: Output directory required.\n");
        PrintUsage();
        return false;
    }
    
    return true;
}

static inline
char *GenerateOutputFilename(const char *InputFile,
                             const char *OutputDirectory,
                             const char *NewExtension)
{
    char *Filename = GetFilename(InputFile);
    char *NewFilename = ReplaceExtension(Filename, NewExtension);
    free(Filename);
    
    char *Result = AppendToDirectory(OutputDirectory, NewFilename);
    free(NewFilename);
    
    return Result;
}

static
bool GenFile(inspect_data *Data,
             const char *TemplatePath,
             const char *OutputFilename)
{
    write_parser WriteParser;
    if (!CreateParser(&WriteParser, TemplatePath, OutputFilename))
    {
        return false;
    }
    
    if (!EvaluateTemplate(&WriteParser, Data->GlobalScope.Dict))
    {
        return false;
    }
    
    puts(OutputFilename);
    
    return true;
}

int main(int argc, char **argv)
{
    command_options Options;
    if (!CreateCommandOptions(argc, argv, &Options))
    {
        return CODEGEN_FAILURE;
    }
    
    if (Options.DoNotRun)
    {
        return CODEGEN_SUCCESS;
    }
    
    inspect_data Data;
    CreateInspectData(&Data);
    
    if (!Options.UseDebugFiles)
    {
        inspect_parser InspectParser;
        if (!CreateParser(Options.InputFile, &InspectParser))
        {
            FreeInspectData(&Data);
            return CODEGEN_FAILURE;
        }
        
        if (!ParseInspect(&InspectParser, &Data))
        {
            FreeInspectData(&Data);
            FreeParser(&InspectParser);
            return CODEGEN_FAILURE;
        }
        
        char *HeaderFileName = GenerateOutputFilename(Options.InputFile,
                                                      Options.OutputDirectory,
                                                      ".gen.h");
        
        char *SourceFileName = GenerateOutputFilename(Options.InputFile,
                                                      Options.OutputDirectory,
                                                      ".gen.cpp");
        
        Insert(Data.GlobalScope.Dict, "HeaderFile",
               ReceiveStringItem(GetFilename(HeaderFileName)));
        Insert(Data.GlobalScope.Dict, "SourceFile",
               ReceiveStringItem(GetFilename(SourceFileName)));
        
        if (!GenFile(&Data, "codegen/templates/data.header", HeaderFileName))
        {
            FreeInspectData(&Data);
            FreeParser(&InspectParser);
            printf("%s -- FAILED\n", HeaderFileName);
            return CODEGEN_FAILURE;
        }
        
        if (!GenFile(&Data, "codegen/templates/data.source", SourceFileName))
        {
            FreeInspectData(&Data);
            FreeParser(&InspectParser);
            printf("%s -- FAILED\n", HeaderFileName);
            return CODEGEN_FAILURE;
        }
        
        free(HeaderFileName);
        free(SourceFileName);
        FreeParser(&InspectParser);
    }
    else
    {
        const char *InputFile = "codegen/debug_files/debug.ins";
        inspect_parser InspectParser;
        if (!CreateParser(InputFile, &InspectParser))
        {
            FreeInspectData(&Data);
            return CODEGEN_FAILURE;
        }
        
        if (!ParseInspect(&InspectParser, &Data))
        {
            FreeInspectData(&Data);
            FreeParser(&InspectParser);
            return CODEGEN_FAILURE;
        }
        
        char *OutputFilename = GenerateOutputFilename(InputFile,
                                                      "codegen/debug_files/",
                                                      ".gen.cpp");
        
        Insert(Data.GlobalScope.Dict, "HeaderFile", NewStringItem("no_header.h"));
        Insert(Data.GlobalScope.Dict, "SourceFile",
               ReceiveStringItem(GetFilename(OutputFilename)));
        
        if (!GenFile(&Data, "codegen/debug_files/debug.template", OutputFilename))
        {
            FreeInspectData(&Data);
            FreeParser(&InspectParser);
            printf("%s -- FAILED\n", OutputFilename);
            return CODEGEN_FAILURE;
        }
        
        free(OutputFilename);
        FreeParser(&InspectParser);
    }
    
    FreeInspectData(&Data);
    return CODEGEN_SUCCESS;
}
