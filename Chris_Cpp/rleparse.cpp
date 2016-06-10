#include "rleparse.h"
#include <filesystem>
#include <Shlwapi.h>
#include <functional> 

extern "C"
{
#include "mpc.h"
}

#pragma comment(lib, "shlwapi.lib")

namespace fs = std::tr2::sys;

// Get EXE Path
TCHAR* GetThisPath(TCHAR* dest, size_t destSize)
{
    if (!dest) return NULL;
    if (MAX_PATH > destSize) return NULL;

    auto length = GetModuleFileName(NULL, dest, DWORD(destSize));
    PathRemoveFileSpec(dest);
    return dest;
}

// Find a list of pattern files
std::vector<fs::path> FindRLE()
{
    std::vector<fs::path> files;
    TCHAR appPath[MAX_PATH];
    GetThisPath(appPath, MAX_PATH);

    fs::path someDir = fs::path(appPath);

    fs::directory_iterator end_iter;
    if (fs::exists(someDir) && fs::is_directory(someDir))
    {
        // Walk the top level directoriespdate
        for (fs::directory_iterator dir_iter(someDir); dir_iter != end_iter; ++dir_iter)
        {
            if (dir_iter->path().extension().generic_string() == ".rle")
            {
                files.push_back(dir_iter->path());
            }
        }
    }
    return files;
}

// Parse all pattern files
std::vector<RLE> GatherPatterns()
{
    std::vector<RLE> patterns;

    // Pattern parser
    mpc_parser_t* ident = mpc_new("ident");
    mpc_parser_t* comment = mpc_new("comment");
    mpc_parser_t* rule = mpc_new("rule");
    mpc_parser_t* number = mpc_new("number");
    mpc_parser_t* assignment = mpc_new("assignment");
    mpc_parser_t* rle = mpc_new("rle");
    mpc_parser_t* rleSymbol = mpc_new("rlesymbol");
    mpc_parser_t* rleNumber = mpc_new("rlenumber");
    mpc_parser_t* fullParse = mpc_new("parser");

    // Here's an example file that this parser reads:
    // #N Gosper glider gun_synth
    // #C Glider synthesis of Gosper glider gun.
    // #C www.conwaylife.com / wiki / index.php ? title = Gosper_glider_gun
    // x = 47, y = 14, rule = b3 / s23
    // 16bo30b$16bobo16bo11b$16b2o17bobo9b$obo10bo21b2o10b$b2o11b2o31b$bo11b
    // 2o32b3$10b2o20b2o13b$11b2o19bobo9b3o$10bo21bo11bo2b$27bo17bob$27b2o18b
    // $26bobo!

    // Grammar
    mpc_err_t* err = mpca_lang(MPCA_LANG_DEFAULT,
        R"tag(
        ident       : /[a-zA-Z_][a-zA-Z0-9_]*/ ;               
        rule        : /[a-zA-Z]*[0-9]+\/[a-zA-Z]*[0-9]+/ ;      
        number      : /[0-9]+/ ;                               
        comment     : /#[^\n]*\n/ ;                            
        assignment  : <ident> '=' (<rule> | <number>) /,*/ ;
        rlesymbol   : /[a-zA-Z]|\$/ ;
        rlenumber   : /[0-9]+/ ;
        rle         : (<rlenumber> <rlesymbol>) | <rlesymbol> ;
        parser      : /^/ <comment>* <assignment>* <rle>* /!/ /$/ ; 
        )tag",
        ident, comment, rule, number, assignment, rle, rleSymbol, rleNumber, fullParse, NULL);

    // Bug in the grammar
    if (err != NULL)
    {
        OutputDebugStringA(mpc_err_string(err));
        mpc_err_print(err);
        mpc_err_delete(err);
        return patterns;
    }

    // Find all the RLE files
    auto found = FindRLE();
    
    // For each pattern, parse the file, walk the AST and build the data structure
    for (auto& patternFile : found)
    {
        // A list of dead/alive cells on each row.
        RLE currentPattern;

        // Parse the file
        mpc_result_t r;
        std::string parseFile = patternFile.string();
        if (mpc_parse_contents(parseFile.c_str(), fullParse, &r))
        {
            // Read the AST
            mpc_ast_t* ast = (mpc_ast_t*)r.output;
            //mpc_ast_print(ast);

            AliveDead cells;
            cells.count = 1;
            std::vector<AliveDead> lineRun;
            std::function<void(mpc_ast_t*)> readAST;

            // Traverse the tree in 'pre' order. 
            mpc_ast_trav_t* traverse = mpc_ast_traverse_start(ast, mpc_ast_trav_order_t::mpc_ast_trav_order_pre);
            auto ast_current = mpc_ast_traverse_next(&traverse);
            while (ast_current)
            {
                std::string tag(ast_current->tag);
                if (tag == "rlenumber|regex")
                {
                    cells.count = std::stoi(ast_current->contents);
                }
                else if (tag == "rlesymbol|regex" ||
                    tag == "rle|rlesymbol|regex")
                {
                    if (ast_current->contents[0] == 'o' ||
                        ast_current->contents[0] == 'x')
                    {
                        cells.alive = 1;
                        lineRun.push_back(cells);
                        cells.count = 1;
                    }
                    else if (ast_current->contents[0] == '$')
                    {
                        currentPattern.runs.push_back(lineRun);
                        lineRun.clear();
                        cells.count--;
                        while (cells.count > 0)
                        {
                            currentPattern.runs.push_back(std::vector<AliveDead>());
                            cells.count--;
                        }
                        cells.count = 1;
                    }
                    else
                    {
                        cells.alive = 0;
                        lineRun.push_back(cells);
                        cells.count = 1;
                    }
                }
                ast_current = mpc_ast_traverse_next(&traverse);
            }
            mpc_ast_traverse_free(&traverse);

            if (!lineRun.empty())
            {
                currentPattern.runs.push_back(lineRun);
            }

            // Delete AST
            mpc_ast_delete(ast);
        }
        else
        {
            OutputDebugStringA(mpc_err_string(r.error));
            mpc_err_delete(r.error);
        }

        // Remember the pattern
        patterns.push_back(currentPattern);
    }

    // Cleanup the parser
    mpc_cleanup(9, ident, comment, rule, number, assignment, rle, rleSymbol, rleNumber, fullParse);

    return patterns;
}