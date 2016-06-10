#define _CRTDBG_MAP_ALLOC
#define NOMINMAX

#include <windows.h>
#include <algorithm>

using namespace std;
#include <gdiplus.h>

#include <memory>
#include "simplexnoise.h"
#include <cassert>
#include "rleparse.h"

#pragma comment (lib,"Gdiplus.lib")
using namespace Gdiplus;

bool clear = false;

struct Cell
{
    uint8_t alive : 1;
    uint8_t age : 7;
};

std::shared_ptr<Bitmap> spBitmap;
std::vector<Cell> buffers[2];
uint32_t currentBuffer = 0;
int currentScale = 1;

int mapWidth = 0;
int mapHeight = 0;
int windowWidth = 0;
int windowHeight = 0;

std::vector<RLE> patterns;
int currentPattern = 0;

bool init = true;
bool pause = false;
bool step = false;

int StepSimulation()
{
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
        currentBuffer = !currentBuffer;
        init = false;
    }
    return !currentBuffer;
}

void CopyTargetToBitmap(int targetBuffer)
{
    if (spBitmap)
    {
        Cell* pDestBuffer = &buffers[targetBuffer][0];

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
    }
}

VOID OnPaint(HDC hdc)
{
    Graphics graphics(hdc);

    graphics.SetSmoothingMode(SmoothingMode::SmoothingModeDefault);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    if (!pause || step)
    {
        int targetBuffer = StepSimulation();
        CopyTargetToBitmap(targetBuffer);
        step = false;
    }

    RectF dest;
    dest.X = 0;
    dest.Y = 0;
    dest.Width = float(windowWidth);
    dest.Height = float(windowHeight);
    graphics.DrawImage(spBitmap.get(), dest, 0.0f, 0.0f, float(mapWidth), float(mapHeight), Unit(UnitPixel));
}

void StampCurrentPattern(int locationX, int locationY)
{
    if (patterns.empty())
    {
        return;
    }

    if (currentPattern >= patterns.size())
    {
        currentPattern = 0;
    }

    Cell* pDestBuffer = &buffers[currentBuffer][0];

    int x = locationX;
    int y = locationY;
    for (auto& run : patterns[currentPattern].runs)
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

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void ShowConsole()
{
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
}

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, PSTR, INT iCmdShow)
{
    HWND                hWnd;
    MSG                 msg;
    WNDCLASS            wndClass;
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR           gdiplusToken;

    // Initialize GDI+.
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    patterns = GatherPatterns();

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

    } while (msg.message != WM_QUIT);

    GdiplusShutdown(gdiplusToken);
    return int(msg.wParam);
}

POINT GetBitmapCursorPos(HWND hWnd)
{
    POINT cursorPt;
    GetCursorPos(&cursorPt);
    RECT rc;
    GetClientRect(hWnd, &rc);
    ScreenToClient(hWnd, &cursorPt);

    // High dpi
    cursorPt.x >>= 1;

    cursorPt.x >>= currentScale;
    cursorPt.y >>= currentScale;

    return cursorPt;
}

void InitMaps()
{
    mapWidth = windowWidth >> currentScale;
    mapHeight = windowHeight >> currentScale;
    spBitmap = std::make_shared<Bitmap>(mapWidth, mapHeight, PixelFormat32bppPARGB);
    buffers[0].resize(mapWidth * mapHeight);
    buffers[1].resize(mapWidth * mapHeight);
    init = true;
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
        StampCurrentPattern(pt.x, pt.y);
        step = true;
    }
    break;
    case WM_CHAR:
    {
        if (wParam == 'c')
        {
            clear = true;
            step = true;
        }
        else if (wParam == 'i')
        {
            init = true;
            step = true;
        }
        else if (wParam == 'n')
        {
            currentPattern++;
        }
        else if (wParam == 's')
        {
            auto pt = GetBitmapCursorPos(hWnd);
            StampCurrentPattern(pt.x, pt.y);
            step = true;
        }
        else if (wParam == 'p')
        {
            pause = !pause;
        }
        else if (wParam == ' ')
        {
            step = true;
        }
        else if (wParam == 'z')
        {
            currentScale--;
            currentScale = std::max(0, currentScale);
            InitMaps();
            step = true;
        }
        else if (wParam == 'x')
        {
            currentScale++;
            currentScale = std::min(6, currentScale);
            InitMaps();
            step = true;
        }
    }
    break;
    case WM_SIZE:
    {
        windowWidth = LOWORD(lParam);
        windowHeight = HIWORD(lParam);
        InitMaps();
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

