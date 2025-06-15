#include <windows.h>
#include <string>
#include <vector>
#include <ctime>
#include <commctrl.h>
#include <gdiplus.h>
#include <uxtheme.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")


using namespace Gdiplus;

// Константы
const int SCREEN_WIDTH = 689;
const int SCREEN_HEIGHT = 760;
const int PLANE_WIDTH = 100;
const int PLANE_HEIGHT = 85;                                 
const int BOMB_WIDTH = 40;
const int BOMB_HEIGHT = 50;
const int GROUND_LEVEL = SCREEN_HEIGHT - 50;

// Глобальные переменные
HWND g_hwnd;
HINSTANCE g_hInstance;
bool g_isRunning = false;
bool g_isGameOver = false;
int g_score = 0;
int g_speed = 3;

// Элементы UI
HWND g_hStartBtn = nullptr;
HWND g_hExitBtn = nullptr;
HWND g_hInfoBtn = nullptr;
HWND g_hRestartBtn = nullptr;
HWND g_hTitleLabel = nullptr;

// Изображения
Gdiplus::Image* g_background = nullptr;
Gdiplus::Image* g_plane = nullptr;
Gdiplus::Image* g_bomb = nullptr;
Gdiplus::Image* g_name = nullptr;

// Позиции
int g_planeX = SCREEN_WIDTH / 2 - PLANE_WIDTH / 2;
const int g_planeY = GROUND_LEVEL - PLANE_HEIGHT;

// Бомбы
struct Bomb {
    int x, y;
    bool active;
};
std::vector<Bomb> g_bombs;

// Таймеры
UINT_PTR g_gameTimer = 0;
UINT_PTR g_bombSpawnTimer = 0;

// Буфер для двойной буферизации
HDC g_memDC = nullptr;
HBITMAP g_memBitmap = nullptr;

// Прототипы функций
void InitGame();
void SpawnBomb();
void UpdateGame();
void DrawGame(HDC hdc);
void CheckCollisions();
void ResetGame();
void ShowMenu();
void ShowGameOver();
void CleanupButtons();
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInstance = hInstance;


    // Инициализация GDI+
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // Загрузка изображений
    g_background = new Image(L"assets/background.png");
    g_plane = new Image(L"assets/plane.png");
    g_bomb = new Image(L"assets/bomb.png");
    g_name = new Image(L"assets/name.png");


    // Регистрация класса окна
    const wchar_t CLASS_NAME[] = L"Літак";

    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);


    RegisterClass(&wc);

    // Создание окна
    g_hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Літак",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        SCREEN_WIDTH + 16, SCREEN_HEIGHT + 39,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (g_hwnd == NULL) return 0;

    // Фиксируем размер окна
    SetWindowLong(g_hwnd, GWL_STYLE, GetWindowLong(g_hwnd, GWL_STYLE) & ~WS_SIZEBOX);

    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);

    // Показываем меню
    ShowMenu();

    // Цикл сообщений
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Освобождение ресурсов
    delete g_background;
    delete g_plane;
    delete g_bomb;
    delete g_name;
    if (g_memDC) DeleteDC(g_memDC);
    if (g_memBitmap) DeleteObject(g_memBitmap);
    GdiplusShutdown(gdiplusToken);

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_KEYDOWN: {
        if (g_isRunning && !g_isGameOver) {
            if (wParam == 'A' && g_planeX > 0) g_planeX -= 15;
            if (wParam == 'D' && g_planeX < SCREEN_WIDTH - PLANE_WIDTH) g_planeX += 15;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        break;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == 1) { // Старт/Рестарт
            CleanupButtons();
            g_isRunning = true;
            g_isGameOver = false;
            ResetGame();
            SetTimer(hwnd, 1, 16, NULL);
            SetTimer(hwnd, 2, 800, NULL);
        }
        else if (LOWORD(wParam) == 2) { // Выход
            PostQuitMessage(0);
        }
        else if (LOWORD(wParam) == 3) { // Информация
            MessageBox(hwnd,
                L"Керування: A - вліво, D - вправо\n"
                L"Мета: уникати бомб і набирати очки\n"
                L"Швидкість збільшується кожні 10 очок\n"
                L"Автор: Константінов Владислав",
                L"Інформація", MB_OK);
        }
        break;
    }
    case WM_TIMER: {
        if (wParam == 1) UpdateGame();
        if (wParam == 2) SpawnBomb();
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        DrawGame(hdc);
        EndPaint(hwnd, &ps);
        break;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        if (g_memDC) DeleteDC(g_memDC);
        if (g_memBitmap) DeleteObject(g_memBitmap);
        g_memDC = nullptr;
        g_memBitmap = nullptr;
        break;
    case WM_DESTROY: {
        KillTimer(hwnd, 1);
        KillTimer(hwnd, 2);
        PostQuitMessage(0);
        break;
    }
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void InitGame() {
    g_bombs.clear();
    g_score = 0;
    g_speed = 20;
    g_planeX = SCREEN_WIDTH / 2 - PLANE_WIDTH / 2;
}

void SpawnBomb() {
    Bomb bomb;
    bomb.x = rand() % (SCREEN_WIDTH - BOMB_WIDTH);
    bomb.y = -BOMB_HEIGHT;
    bomb.active = true;
    g_bombs.push_back(bomb);
}

void UpdateGame() {
    if (!g_isRunning || g_isGameOver) return;

    // Движение бомб
    for (auto& bomb : g_bombs) {
        if (bomb.active) {
            bomb.y += g_speed;
            if (bomb.y > SCREEN_HEIGHT) {
                bomb.active = false;
                g_score++;
                if (g_score % 10 == 0) g_speed++;
            }
        }
    }

    // Удаление неактивных бомб
    g_bombs.erase(
        std::remove_if(
            g_bombs.begin(),
            g_bombs.end(),
            [](const Bomb& bomb) { return !bomb.active; }
        ),
        g_bombs.end()
    );

    CheckCollisions();
    InvalidateRect(g_hwnd, NULL, FALSE);
}

void DrawGame(HDC hdc) {
    // Создаем буфер, если его нет
    if (!g_memDC) {
        g_memDC = CreateCompatibleDC(hdc);
        g_memBitmap = CreateCompatibleBitmap(hdc, SCREEN_WIDTH, SCREEN_HEIGHT);
        SelectObject(g_memDC, g_memBitmap);
    }

    Graphics graphics(g_memDC);

    // Очищаем буфер
    graphics.Clear(Color(255, 135, 206, 235)); // Голубой фон

    // Рисуем фон
    if (g_background)
        graphics.DrawImage(g_background, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Рисуем название/лого, если игра не запущена
    if (!g_isRunning && !g_isGameOver && g_name) {
        int nameWidth = 300;
        int nameHeight = 200;
        int nameX = SCREEN_WIDTH / 2 - nameWidth / 2;
        int nameY = 150;
        graphics.DrawImage(g_name, nameX, nameY, nameWidth, nameHeight);
    }
    // Рисуем самолет
    if (g_plane)
        graphics.DrawImage(g_plane, g_planeX, g_planeY, PLANE_WIDTH, PLANE_HEIGHT);

    // Рисуем бомбы
    if (g_bomb) {
        for (const auto& bomb : g_bombs) {
            if (bomb.active)
                graphics.DrawImage(g_bomb, bomb.x, bomb.y, BOMB_WIDTH, BOMB_HEIGHT);
        }
    }

    // Очки (только во время игры)
    if (g_isRunning && !g_isGameOver) {
        Font font(L"assets/2d.ttf", 24);
        SolidBrush brush(Color(255, 255, 255));
        graphics.DrawString((L"Очки: " + std::to_wstring(g_score)).c_str(), -1, &font, PointF(20, 20), &brush);
    }

    // Game Over
    if (g_isGameOver) {
        Font bigFont(L"assets/2d.ttf", 48);
        SolidBrush redBrush(Color(255, 255, 0, 0));
        graphics.DrawString(L"GAME OVER", -1, &bigFont, PointF(SCREEN_WIDTH / 2 - 200, SCREEN_HEIGHT / 2 - 250), &redBrush);

        SolidBrush brush(Color(255, 255, 255));
        Font scoreFont(L"assets/2d.ttf", 36);
        graphics.DrawString((L"Score: " + std::to_wstring(g_score)).c_str(), -1, &scoreFont, PointF(SCREEN_WIDTH / 2 - 90, SCREEN_HEIGHT / 2 - 150), &brush);
    }

    // Копируем буфер на экран
    BitBlt(hdc, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, g_memDC, 0, 0, SRCCOPY);
}

void CheckCollisions() {
    Bitmap planeBitmap(PLANE_WIDTH, PLANE_HEIGHT);
    Graphics planeGraphics(&planeBitmap);
    planeGraphics.DrawImage(g_plane, 0, 0, PLANE_WIDTH, PLANE_HEIGHT);

    Bitmap bombBitmap(BOMB_WIDTH, BOMB_HEIGHT);
    Graphics bombGraphics(&bombBitmap);
    bombGraphics.DrawImage(g_bomb, 0, 0, BOMB_WIDTH, BOMB_HEIGHT);

    for (auto& bomb : g_bombs) {
        if (!bomb.active) continue;

        if (g_planeX < bomb.x + BOMB_WIDTH && g_planeX + PLANE_WIDTH > bomb.x &&
            g_planeY < bomb.y + BOMB_HEIGHT && g_planeY + PLANE_HEIGHT > bomb.y) {

            for (int bx = 0; bx < BOMB_WIDTH; bx++) {
                for (int by = 0; by < BOMB_HEIGHT; by++) {
                    Color bombPixel;
                    bombBitmap.GetPixel(bx, by, &bombPixel);
                    if (bombPixel.GetAlpha() > 0) {
                        int px = bx + bomb.x - g_planeX;
                        int py = by + bomb.y - g_planeY;

                        if (px >= 0 && px < PLANE_WIDTH && py >= 0 && py < PLANE_HEIGHT) {
                            Color planePixel;
                            planeBitmap.GetPixel(px, py, &planePixel);
                            if (planePixel.GetAlpha() > 0) {
                                g_isGameOver = true;
                                KillTimer(g_hwnd, 1);
                                KillTimer(g_hwnd, 2);
                                ShowGameOver();
                                return;
                            }
                        }
                    }
                }
            }
        }
    }
}

void ResetGame() {
    InitGame();
    InvalidateRect(g_hwnd, NULL, TRUE);
}

void CleanupButtons() {
    if (g_hStartBtn) DestroyWindow(g_hStartBtn);
    if (g_hExitBtn) DestroyWindow(g_hExitBtn);
    if (g_hInfoBtn) DestroyWindow(g_hInfoBtn);
    if (g_hRestartBtn) DestroyWindow(g_hRestartBtn);
    g_hStartBtn = g_hExitBtn = g_hInfoBtn = g_hRestartBtn = nullptr;
}

void ShowMenu() {
    CleanupButtons();

    g_hStartBtn = CreateWindow(
        L"BUTTON", L"Старт",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        SCREEN_WIDTH / 2 - 100, 340, 200, 50,
        g_hwnd, (HMENU)1, g_hInstance, NULL
    );

    g_hInfoBtn = CreateWindow(
        L"BUTTON", L"Інформація",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        SCREEN_WIDTH / 2 - 100, 410, 200, 50,
        g_hwnd, (HMENU)3, g_hInstance, NULL
    );

    g_hExitBtn = CreateWindow(
        L"BUTTON", L"Вихід",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        SCREEN_WIDTH / 2 - 100, 480, 200, 50,
        g_hwnd, (HMENU)2, g_hInstance, NULL
    );
}

void ShowGameOver() {
    CleanupButtons();

    g_hRestartBtn = CreateWindow(
        L"BUTTON", L"Заново",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        SCREEN_WIDTH / 2 - 100, 300, 200, 50,
        g_hwnd, (HMENU)1, g_hInstance, NULL
    );

    g_hExitBtn = CreateWindow(
        L"BUTTON", L"Вихід",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        SCREEN_WIDTH / 2 - 100, 370, 200, 50,
        g_hwnd, (HMENU)2, g_hInstance, NULL
    );
}