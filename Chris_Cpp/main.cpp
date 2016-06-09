#define _CRTDBG_MAP_ALLOC
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <memory>
#include <vector>
#include "simplexnoise.h"
#include <filesystem>
#include <Shlwapi.h>
#include <fstream>
#include <string>
#include <sstream>
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>
#include <cassert>
#pragma comment(lib, "shlwapi.lib")

using namespace Gdiplus;
#pragma comment (lib,"Gdiplus.lib")

namespace fs = std::tr2::sys;

bool clear = false;

struct Cell
{
    uint8_t alive : 1;
    uint8_t age : 7;
};

std::shared_ptr<Bitmap> spBitmap;
std::vector<Cell> buffers[2];
uint32_t currentBuffer = 0;

int mapWidth = 100;
int mapHeight = 100;

int windowWidth = 0;
int windowHeight = 0;

int currentRLE = 0;
std::vector<fs::path> rleFiles;

void AddRLE(int locationX, int locationY);

bool init = true;
VOID OnPaint(HDC hdc)
{
    Graphics graphics(hdc);
    
    graphics.SetSmoothingMode(SmoothingMode::SmoothingModeDefault);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
 
    // Lock the bitmap with the same format, to avoid translation, draw a green line on it, and copy it to the screen.
    if (spBitmap && !buffers[0].empty())
    {
        Cell* pSourceBuffer = &buffers[currentBuffer][0];
        Cell* pDestBuffer = &buffers[!currentBuffer][0];
        
        for (int y = 0; y < (int)mapHeight; y++)
        {
            for (int x = 0; x < (int)mapWidth; x++)
            {
                int surroundCount = 0;
                if (init)
                {
                    auto fVal = octave_noise_2d(4.0f, .9f, 2.0f / float(mapWidth), x, y);

                    if (fVal > .1f)
                    {
                        surroundCount = 3;
                    }
                }
                else
                {
                    for (int offsetY = -1; offsetY <= 1; offsetY++)
                    {
                        for (int offsetX = -1; offsetX <= 1; offsetX++)
                        {
                            if (offsetX == 0 && offsetY == 0)
                                continue;

                            // Offset with wrap
                            int newX = x + offsetX;
                            int newY = y + offsetY;
                            if (newX < 0) newX = mapWidth + newX;
                            if (newY < 0) newY = mapHeight + newY;
                            if (newX >= mapWidth) newX = newX - mapWidth;
                            if (newY >= mapHeight) newY = newY - mapHeight;
                            
                            Cell* pSourceData = (pSourceBuffer + (newY * mapWidth) + (newX));

                            surroundCount += pSourceData->alive;
                        }
                    }
                }
             
                // Pixel Maths.... step down by the x stride to the row, then across to the pixel
                // We work in 4 byte pieces, because each pixel is ARGB 
                Cell* pTarget = (pDestBuffer + (y * mapWidth) + (x));
                Cell* pSource = (pSourceBuffer + (y * mapWidth) + (x));
                bool live = (pSource->alive);
                if (live)
                {
                    if (surroundCount == 2 || 
                        surroundCount == 3)
                    {
                        // Under population death
                        pTarget->alive = 1;
                        pTarget->age = pSource->age;
                    }
                    else
                    {
                        // Over population death
                        pTarget->alive = 0;
                    }
                }
                else
                {
                    if (surroundCount == 3 /*||
                        surroundCount == 6*/)
                    {
                        // New life from death
                        pTarget->alive = 1;
                        pTarget->age = 0;
                    }
                    else
                    {
                        *pTarget = *pSource;
                    }
                }

                if (pTarget->age < 100)
                {
                    pTarget->age++;
                }

                if (clear == true)
                {
                    pTarget->alive = 0;
                }
            }
        }

        clear = false;

        BitmapData writeData;
        Rect lockRect(0, 0, mapWidth, mapHeight);
        spBitmap->LockBits(&lockRect, ImageLockModeWrite, PixelFormat32bppPARGB, &writeData);

        Cell* pData = &buffers[!currentBuffer][0];
        for (int y = 0; y < int(writeData.Height); y++)
        {
            for (auto x = 0; x < int(writeData.Width); x++)
            {

                uint32_t* pTarget = (uint32_t*)((uint8_t*)writeData.Scan0 + (y * writeData.Stride) + (x * 4));
                Cell* pSource = (pDestBuffer + uint32_t(y * mapWidth) + uint32_t(x));
                uint32_t age = (uint8_t)(255.0f * ((float)pSource->age / 100.0f));
                if (pSource->alive)
                {
                    if (age <= 4)
                    {
                        *pTarget = 0xFFFFFFFF;
                    }
                    else
                    {
                        *pTarget = 0xFF000000 | (age << 16) | ((0xFF - age) << 8);
                    }
                }
                else
                {
                    *pTarget = 0xFF000000;
                }
            }
        }

        spBitmap->UnlockBits(&writeData);
        
        RectF dest;
        dest.X = 0;
        dest.Y = 0;
        dest.Width = float(windowWidth);
        dest.Height = float(windowHeight);
        graphics.DrawImage(spBitmap.get(), dest, 0.0f, 0.0f, float(mapWidth), float(mapHeight), Unit(UnitPixel) );
        currentBuffer = !currentBuffer;
        init = false;
    }
}

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, PSTR, INT iCmdShow)
{
    HWND                hWnd;
    MSG                 msg;
    WNDCLASS            wndClass;
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR           gdiplusToken;

    // Initialize GDI+.
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    wndClass.style = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc = WndProc;
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hInstance = hInstance;
    wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wndClass.lpszMenuName = NULL;
    wndClass.lpszClassName = TEXT("GettingStarted");

    RegisterClass(&wndClass);

    hWnd = CreateWindow(
        TEXT("GettingStarted"),   // window class name
        TEXT("Getting Started"),  // window caption
        WS_OVERLAPPEDWINDOW,      // window style
        CW_USEDEFAULT,            // initial x position
        CW_USEDEFAULT,            // initial y position
        1024,            // initial x size
        768,            // initial y size
        NULL,                     // parent window handle
        NULL,                     // window menu handle
        hInstance,                // program instance handle
        NULL);                    // creation parameters

    ShowWindow(hWnd, iCmdShow);
    UpdateWindow(hWnd);


    HDC dc = GetDC(hWnd);
    do
    {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        OnPaint(dc);
        //EndPaint(hWnd, &ps);

    } while (msg.message != WM_QUIT);

    GdiplusShutdown(gdiplusToken);
    return int(msg.wParam);
}  // WinMain

POINT GetBitmapCursorPos(HWND hWnd)
{
    POINT cursorPt;
    GetCursorPos(&cursorPt);
    RECT rc;
    GetClientRect(hWnd, &rc);
    ScreenToClient(hWnd, &cursorPt);

    // High dpi
    cursorPt.x >>= 2;
    cursorPt.y >>= 1;

    return cursorPt;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    HDC          hdc;
    PAINTSTRUCT  ps;

    switch (message)
    {
    case WM_LBUTTONDOWN:
    {
        auto pt = GetBitmapCursorPos(hWnd);
        AddRLE(pt.x, pt.y);
    }
    break;
    case WM_CHAR:
    {
        if (wParam == 'c')
        {
            clear = true;
        }
        else if (wParam == 'i')
        {
            init = true;
        }
        else if (wParam == 'n')
        {
            currentRLE++;
        }
        else if (wParam == 's')
        {
            auto pt = GetBitmapCursorPos(hWnd);
            AddRLE(pt.x, pt.y);
        }
    }
    break;
    case WM_SIZE:
    {
        windowWidth = LOWORD(lParam);
        windowHeight = HIWORD(lParam);
        mapWidth = windowWidth >> 1;
        mapHeight = windowHeight >> 1;
        spBitmap = std::make_shared<Bitmap>(mapWidth, mapHeight, PixelFormat32bppPARGB);
        buffers[0].resize(mapWidth * mapHeight);
        buffers[1].resize(mapWidth * mapHeight);

        init = true;
    }
        return 0;
    case WM_ERASEBKGND:
        return TRUE;

    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        OnPaint(hdc);
        EndPaint(hWnd, &ps);
        return 0;
    case WM_DESTROY:
        spBitmap.reset();
        PostQuitMessage(0);
        return 0; 
    default:
        break;  
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
} // WndProc

TCHAR* GetThisPath(TCHAR* dest, size_t destSize)
{
    if (!dest) return NULL;
    if (MAX_PATH > destSize) return NULL;

    auto length = GetModuleFileName(NULL, dest, DWORD(destSize));
    PathRemoveFileSpec(dest);
    return dest;
}
 
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

static inline std::string &ltrim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
}

// trim from end
static inline std::string &rtrim(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
}

// trim from both ends
static inline std::string &trim(std::string &s)
{
    return ltrim(rtrim(s));
}
void AddRLE(int locationX, int locationY)
{
    auto found = FindRLE();
    if (found.empty())
    {
        return;
    }

    if (currentRLE >= found.size())
    {
        currentRLE = 0;
    }

    auto readFile = [](fs::path filePath)
    {
        std::ifstream ifs(filePath);
        std::ostringstream buffer;
        buffer << ifs.rdbuf();
        return buffer;
    };

    auto tokenize = [](std::string str, std::vector<std::string> &token_v, const char delim = ' ')
    {
        size_t start = str.find_first_not_of(delim), end = start;

        while (start != std::string::npos)
        {
            // Find next occurence of delimiter
            end = str.find(delim, start);
            // Push back the token found into vector
            
            std::string tok = str.substr(start, end - start);
            tok.erase(std::remove(tok.begin(), tok.end(), ','), tok.end());

            token_v.push_back(tok);
            // Skip all occurences of the delimiter to find new start
            start = str.find_first_not_of(delim, end);
        }
    };

    auto RemoveLine = [](std::string& source, const std::string& to_remove)
    {
        size_t m = source.find(to_remove);
        size_t n = source.find_first_of("\n", m + to_remove.length());
        source.erase(m, n - m + 1);
    };

    struct AliveDead
    {
        int count;
        bool alive;
    };
    struct RLE
    {
        int x;
        int y;
        std::vector<std::vector<AliveDead>> runs;
    };

    RLE rle;
    int currentRun = 0;
    std::string line;
    std::ifstream ifs(found[currentRLE]);

    std::vector<AliveDead> lineRun;

    while (std::getline(ifs, line))
    {
        std::vector<std::string> tokens;
        tokenize(line, tokens, ' ');
        int token = 0;
        while(token < tokens.size())
        {
            if (tokens[token].empty())
                continue;

            // Ignore comment lines
            if (tokens[token][0] == '#')
                break;

            if (tokens[token][0] == 'x' &&
                ((tokens.size() > token + 1)) &&
                tokens[token + 1][0] == '=')
            {
                rle.x = std::atoi(tokens[token + 2].c_str());
                token += 3;
                continue;
            }
            if (tokens[token][0] == 'y')
            {
                rle.y = std::atoi(tokens[token + 2].c_str());
                token += 3;
                continue;
            }

            if (tokens[token] == "rule")
            {
                token += 3;
                continue;
            }

            std::size_t pos = 0;
            while (pos < tokens[token].size())
            {
                while (pos < tokens[token].size() &&
                    tokens[token][pos] != '\n' &&
                    tokens[token][pos] != '$' &&
                    tokens[token][pos] != '!')
                {
                    if (tokens[token][pos] == 'o' ||
                        tokens[token][pos] == 'x')
                    {
                        lineRun.push_back(AliveDead{ 1, true });
                        pos++;
                    }
                    else if (tokens[token][pos] == 'b')
                    {
                        lineRun.push_back(AliveDead{ 1, false });
                        pos++;
                    }
                    else
                    {
                        auto endNum = tokens[token].find_first_not_of("0123456789", pos);

                        if (endNum != pos)
                        {
                            AliveDead cells;
                            std::string digit = tokens[token].substr(pos, endNum - pos);
                            cells.count = std::stoi(digit);
                            if (tokens[token][endNum] == 'o' ||
                                tokens[token][endNum] == 'x')
                            {
                                cells.alive = true;
                                pos = endNum + 1;
                                lineRun.push_back(cells);
                            }
                            else if (tokens[token][endNum] == 'b')
                            {
                                cells.alive = false;
                                pos = endNum + 1;
                                lineRun.push_back(cells);
                            }
                            else if (tokens[token][endNum] == '$')
                            {
                                if (!lineRun.empty())
                                {
                                    rle.runs.push_back(lineRun);
                                }
                                lineRun.clear();
                                cells.count--;
                                if (cells.count > 0)
                                {
                                    for (auto i = 0; i < cells.count; i++)
                                    {
                                        rle.runs.push_back(lineRun);
                                    }
                                }
                                pos = endNum + 1;
                            }
                        }
                        else
                        {
                            pos++;
                        }
                    }
                }
                if ((tokens[token][pos] == '!' ||
                    tokens[token][pos] == '$') &&
                    !lineRun.empty())
                {
                    rle.runs.push_back(lineRun);
                    lineRun.clear();
                }
                

                pos++;
            }
        
            token++;
        }
    }

    Cell* pDestBuffer = &buffers[currentBuffer][0];

    int x = locationX;
    int y = locationY;
    for (auto& run : rle.runs)
    {
        if (y >= mapHeight) { y = y - mapHeight; }
        if (x >= mapWidth) { x = x - mapWidth; }

        Cell* pTarget = (Cell*)((Cell*)pDestBuffer + (y * mapWidth) + (x));

        for (auto& row : run)
        {
            for (auto count = 0; count < row.count; count++)
            {
                if ((locationX + x) >= mapWidth) x = x - mapWidth;

                pTarget[x].alive = row.alive ? 1 : 0;
                pTarget[x++].age = 0;

                assert((pTarget + x) <= (pDestBuffer + (mapWidth * mapHeight)));
            }
        }

        y++;
        x = locationX;
    }
}


