/**********************************************************************************
// Window (C�digo Fonte)
// 
// Cria��o:     19 Mai 2007
// Atualiza��o:	26 Jul 2023
// Compilador:	Visual C++ 2022
//
// Descri��o:   Abstrai os detalhes de configura��o de uma janela
//
**********************************************************************************/

#include "Window.h"

// -------------------------------------------------------------------------------
// inicializa��o de membros est�ticos da classe

void (*Window::inFocus)()     = nullptr;                            // nenhuma a��o ao ganhar foco
void (*Window::lostFocus)()   = nullptr;                            // nenhuma a��o ao perder foco
int Window::screenWidth       = 0;                                  // largura da tela
int Window::screenHeight      = 0;                                  // altura da tela

// -------------------------------------------------------------------------------
// construtor

Window::Window()
{
    windowId          = 0;                                           // id nulo porque a janela ainda n�o existe
    screenWidth       = GetSystemMetrics(SM_CXSCREEN);               // largura da tela
    screenHeight      = GetSystemMetrics(SM_CYSCREEN);               // altura da tela
    windowWidth       = screenWidth;                                 // janela ocupa toda a tela (tela cheia)
    windowHeight      = screenHeight;                                // janela ocupa toda a tela (tela cheia)
    windowIcon        = LoadIcon(NULL, IDI_APPLICATION);             // �cone padr�o de uma aplica��o
    windowCursor      = LoadCursor(NULL, IDC_ARROW);                 // cursor padr�o de uma aplica��o
    windowColor       = RGB(0,0,0);                                  // cor de fundo padr�o � preta
    windowTitle       = string("Windows App");                       // t�tulo padr�o da janela
    windowStyle       = WS_POPUP | WS_VISIBLE;                       // estilo para tela cheia
    windowMode        = FULLSCREEN;                                  // modo padr�o � tela cheia
    windowPosX        = 0;                                           // posi��o inicial da janela no eixo x
    windowPosY        = 0;                                           // posi��o inicial da janela no eixo y
    windowCenterX     = windowWidth/2;                               // centro da janela no eixo x
    windowCenterY     = windowHeight/2;                              // centro da janela no eixo y
    windowHdc         = { 0 };                                       // contexto do dispositivo
    windowRect        = { 0, 0, 0, 0 };                              // �rea cliente da janela
    windowAspectRatio = windowWidth / float(windowHeight);           // aspect ratio da janela
    fullWidth         = windowWidth;                                 // largura da janela incluindo bordas
    fullHeight        = windowHeight;                                // altura da janela incluindo barras e bordas
    fullAspectRatio   = fullWidth / float(fullHeight);               // aspect ratio da janela com barras e bordas
    resizeMode        = UNLOCKED;                                    // janela pode ser redimensionada livremente
    styleWidth        = 0;                                           // largura devido a bordas e barras da janela
    styleHeight       = 0;                                           // altura devido a bordas e barras da janela
    windowMinWidth    = 0;                                           // largura m�nima da janela
    windowMinHeight   = 0;                                           // altura m�nima da janela
}

// -------------------------------------------------------------------------------

Window::~Window()
{
    // libera contexto do dispositivo
    if (windowHdc) ReleaseDC(windowId, windowHdc);
}

// -------------------------------------------------------------------------------

void Window::Mode(int mode)
{
    windowMode = mode;

    if (windowMode == WINDOWED)
    {
        // modo em janela
        switch (resizeMode)
        {
        case UNLOCKED:
            windowStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
            break;
        case LOCKED:
            windowStyle = WS_OVERLAPPED | WS_SYSMENU | WS_VISIBLE;
            break;
        case ASPECTRATIO:
            windowStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
            windowStyle -= WS_MAXIMIZEBOX;
            break;
        } 
    }
    else
    {
        // modo em tela cheia ou janela sem bordas
        windowStyle = WS_EX_TOPMOST | WS_POPUP | WS_VISIBLE; 
    } 
}

// -------------------------------------------------------------------------------


void Window::ResizeMode(int mode)
{
    // altera modo de redimensionamento
    resizeMode = mode;

    // reavalia os modos de janela
    Mode(windowMode);
}

// -------------------------------------------------------------------------------

void Window::Size(int width, int height)
{ 
    // tamanho da janela
    fullWidth = windowWidth = width;
    fullHeight = windowHeight = height;

    // tamanho m�nimo da janela
    windowMinWidth = windowWidth / 2;
    windowMinHeight = windowHeight / 2;

    // aspect ratio da janela com barras e bordas
    fullAspectRatio = fullWidth / float(fullHeight);

    // aspect ratio da janela
    windowAspectRatio = windowWidth / float(windowHeight);

    // posi��o do centro da janela
    windowCenterX = windowWidth/2;
    windowCenterY = windowHeight/2;

    // ajusta a posi��o da janela para o centro da tela
    windowPosX = (screenWidth/2) - (windowWidth/2);
    windowPosY = (screenHeight/2) - (windowHeight/2);
}

// -------------------------------------------------------------------------------

void Window::Resize(int width, int height)
{
    // tamanho da �rea cliente da janela
    windowWidth = width;
    windowHeight = height;

    // aspect ratio da janela
    windowAspectRatio = windowWidth / float(windowHeight);

    // tamanho da janela
    fullWidth = width + styleWidth;
    fullHeight = height + styleHeight;

    // aspect ratio da janela com bordas e barras
    fullAspectRatio = fullWidth / float(fullHeight);

    // posi��o do centro da janela
    windowCenterX = windowWidth / 2;
    windowCenterY = windowHeight / 2;

    // ajusta a posi��o da janela para o centro da tela
    windowPosX = (screenWidth / 2) - (windowWidth / 2);
    windowPosY = (screenHeight / 2) - (windowHeight / 2);
}

// -------------------------------------------------------------------------------

void Window::FullResize(int width, int height)
{
    // tamanho da janela
    fullWidth = width;
    fullHeight = height;

    // tamanho da �rea cliente da janela
    windowWidth = width - styleWidth;
    windowHeight = height - styleHeight;

    // aspect ratio da janela
    windowAspectRatio = windowWidth / float(windowHeight);

    // posi��o do centro da janela
    windowCenterX = windowWidth / 2;
    windowCenterY = windowHeight / 2;

    // ajusta a posi��o da janela para o centro da tela
    windowPosX = (screenWidth / 2) - (windowWidth / 2);
    windowPosY = (screenHeight / 2) - (windowHeight / 2);
}

// -------------------------------------------------------------------------------

bool Window::Create()
{
    // identificador da aplica��o
    HINSTANCE appId = GetModuleHandle(NULL);
    
    // definindo uma classe de janela
    WNDCLASSEX wndClass;     
    wndClass.cbSize        = sizeof(WNDCLASSEX);
    wndClass.style         = CS_DBLCLKS | CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc   = Window::WinProc;
    wndClass.cbClsExtra    = 0;
    wndClass.cbWndExtra    = 0;
    wndClass.hInstance     = appId;
    wndClass.hIcon         = windowIcon;
    wndClass.hCursor       = windowCursor; 
    wndClass.hbrBackground = (HBRUSH) CreateSolidBrush(windowColor);
    wndClass.lpszMenuName  = NULL;
    wndClass.lpszClassName = "AppWindow";
    wndClass.hIconSm       = windowIcon;

    // registrando classe "AppWindow"
    if (!RegisterClassEx(&wndClass))
        return false;

    // criando uma janela baseada na classe "AppWindow" 
    windowId = CreateWindowEx(
        NULL,                           // estilos extras
        "AppWindow",                    // nome da classe da janela
        windowTitle.c_str(),            // t�tulo da janela
        windowStyle,                    // estilo da janela
        windowPosX, windowPosY,         // posi��o (x,y) inicial
        windowWidth, windowHeight,      // largura e altura da janela
        NULL,                           // identificador da janela pai
        NULL,                           // identificador do menu
        appId,                          // identificador da aplica��o
        NULL);                          // par�metros de cria��o

    // Ao usar o modo em janela � preciso levar em conta que as barras 
    // e bordas ocupam espa�o na janela. O c�digo abaixo ajusta o tamanho
    // da janela de forma que a �rea cliente fique com tamanho 
    // (windowWidth x windowHeight)

    if (windowMode == WINDOWED)
    {
        // ret�ngulo com o tamanho da �rea cliente desejada
        RECT rect = {0, 0, windowWidth, windowHeight};

        // ajusta o tamanho do ret�ngulo
        AdjustWindowRectEx(&rect,
            GetWindowStyle(windowId),
            GetMenu(windowId) != NULL,
            GetWindowExStyle(windowId));

        // novas dimens�es da janela
        fullWidth = rect.right - rect.left;
        fullHeight = rect.bottom - rect.top;

        // dimens�es dos estilos da janela
        styleWidth = fullWidth - windowWidth;
        styleHeight = fullHeight - windowHeight;

        // ajusta a posi��o da janela para o centro da tela
        windowPosX = (screenWidth/2) - (fullWidth/2);
        windowPosY = (screenHeight/2) - (fullHeight/2);

        // aspect ratio da janela (com barras e bordas)
        fullAspectRatio = fullWidth / float(fullHeight);

        // redimensiona janela com uma chamada a MoveWindow
        MoveWindow(
            windowId,       // identificador da janela
            windowPosX,     // posi��o x
            windowPosY,     // posi��o y
            fullWidth,      // largura
            fullHeight,     // altura
            TRUE);          // repintar
    }

    // captura contexto do dispositivo
    windowHdc = GetDC(windowId);

    // pega tamanho da �rea cliente
    GetClientRect(windowId, &windowRect);

    // retorna estado da inicializa��o (bem sucedida ou n�o)
    return (windowId ? true : false);
}

// -------------------------------------------------------------------------------

LRESULT CALLBACK Window::WinProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    // janela perdeu o foco
    case WM_KILLFOCUS:
        if (lostFocus)
            lostFocus();
        return 0;

    // janela recebeu o foco
    case WM_SETFOCUS:
        if (inFocus)
            inFocus();
        return 0;

    // a janela foi destruida
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// -----------------------------------------------------------------------------
