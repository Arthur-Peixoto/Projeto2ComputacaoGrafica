/**********************************************************************************
// Engine (C�digo Fonte)
//
// Cria��o:		15 Mai 2014
// Atualiza��o:	26 Jul 2023
// Compilador:	Visual C++ 2022
//
// Descri��o:	A Engine roda aplica��es criadas a partir da classe App.
//              Para usar a Engine crie uma inst�ncia e chame o m�todo
//				Start() com um objeto derivado da classe App.
//
**********************************************************************************/

#include "Engine.h"
#include <windows.h>
#include <sstream>
using std::stringstream;

// ------------------------------------------------------------------------------
// Inicializa��o de vari�veis est�ticas da classe

Graphics* Engine::graphics  = nullptr;	// dispositivo gr�fico
Window*   Engine::window    = nullptr;	// janela da aplica��o
Input*    Engine::input     = nullptr;	// dispositivos de entrada
App*      Engine::app       = nullptr;	// apontadador da aplica��o
double    Engine::frameTime = 0.0;		// tempo do quadro atual
bool      Engine::paused    = false;	// estado do motor
Timer     Engine::timer;				// medidor de tempo

// -------------------------------------------------------------------------------

Engine::Engine()
{
	window = new Window();
	graphics = new Graphics();
}

// -------------------------------------------------------------------------------

Engine::~Engine()
{
	delete app;
	delete graphics;
	delete input;
	delete window;
}

// -----------------------------------------------------------------------------

int Engine::Start(App * application)
{
	app = application;

	// cria janela da aplica��o
	window->Create();

	// inicializa dispositivos de entrada (deve ser feito ap�s cria��o da janela)
	input = new Input();

	// inicializa dispositivo gr�fico
	graphics->Initialize(window);

	// altera a window procedure da janela ativa para EngineProc
	SetWindowLongPtr(window->Id(), GWLP_WNDPROC, (LONG_PTR)EngineProc);

	return Loop();
}

// -----------------------------------------------------------------------------

double Engine::FrameTime()
{

#ifdef _DEBUG
	static double totalTime = 0.0;	// tempo total transcorrido 
	static uint   frameCount = 0;	// contador de frames transcorridos
#endif

	// tempo do frame atual
	frameTime = timer.Reset();

#ifdef _DEBUG
	// tempo acumulado dos frames
	totalTime += frameTime;

	// incrementa contador de frames
	frameCount++;

	// a cada 1000ms (1 segundo) atualiza indicador de FPS na janela
	if (totalTime >= 1.0)
	{
		stringstream text;			// fluxo de texto para mensagens
		text << std::fixed;			// sempre mostra a parte fracion�ria
		text.precision(3);			// tr�s casas depois da v�rgula

		text << window->Title().c_str() << "    "
			<< "FPS: " << frameCount << "    "
			<< "Frame Time: " << frameTime * 1000 << " (ms)";

		SetWindowText(window->Id(), text.str().c_str());

		frameCount = 0;
		totalTime -= 1.0;
	}
#endif

	return frameTime;
}

// -------------------------------------------------------------------------------

int Engine::Loop()
{
	// inicia contagem de tempo
	timer.Start();
	
	// mensagens do Windows
	MSG msg = { 0 };
	
	// inicializa��o da aplica��o
	app->Init();

	// la�o principal
	do
	{
		// trata todos os eventos antes de atualizar a aplica��o
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			// -----------------------------------------------
			// Pausa/Resume Jogo
			// -----------------------------------------------

			if (input->KeyPress(VK_PAUSE))
			{
				if (paused)
					Resume();
				else
					Pause();
			}

			// -----------------------------------------------

			if (!paused)
			{
				// calcula o tempo do quadro
				frameTime = FrameTime();

				// atualiza��o da aplica��o 
				app->Update();

				// desenho da aplica��o
				app->Draw();
			}
			else
			{
				app->OnPause();
			}
		}

	} while (msg.message != WM_QUIT);

	// finaliza��o do aplica��o
	app->Finalize();	

	// encerra aplica��o
	return int(msg.wParam);
}

// -------------------------------------------------------------------------------

LRESULT CALLBACK Engine::EngineProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_PAINT:
		// janela precisa ser repintada
		app->Display();
		break;
		
	case WM_GETMINMAXINFO:
	{
		// ajusta tamanho m�nimo da janela
		LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
		lpMMI->ptMinTrackSize.x = window->MinWidth();
		lpMMI->ptMinTrackSize.y = window->MinHeight();
		return 0;
	}

	case WM_SIZE:
	{
		if (wParam == SIZE_MAXIMIZED || wParam == SIZE_RESTORED)
		{
			UINT width = LOWORD(lParam);
			UINT height = HIWORD(lParam);
			window->Resize(width, height);
		}
		return 0;
	}

	case WM_SIZING:
	{
		// novas dimens�es da janela
		LPRECT resize = (LPRECT)lParam;
		uint width = resize->right - resize->left;
		uint height = resize->bottom - resize->top;

		// redimensionamento da janela
		if (window->ResizeMode() == ASPECTRATIO)
		{
			// mant�m o aspect ratio original
			// de acordo com o canto que est�
			// sendo usado para redimensionar
			switch (wParam)
			{
			case WMSZ_TOPLEFT:
			case WMSZ_TOPRIGHT:
				// calcula coordenada para manter o aspect ratio
				height = uint(width / window->FullAspectRatio());
				resize->top = resize->bottom - height;

				// se a coordenada saiu da tela
				if (resize->top < 0)
				{
					// mant�m a janela dentro da tela
					resize->top = 0;

					// calcula nova largura para manter o aspect ratio
					height = resize->bottom - resize->top;
					width = uint(height * window->FullAspectRatio());

					// ajusta largura de acordo com o lado
					if (wParam == WMSZ_TOPLEFT)
						resize->left = resize->right - width;
					else
						resize->right = resize->left + width;
				}
				break;

			case WMSZ_BOTTOMLEFT:
			case WMSZ_BOTTOMRIGHT:
			case WMSZ_LEFT:
			case WMSZ_RIGHT:
				// calcula coordenada para manter o aspect ratio
				height = uint(width / window->FullAspectRatio());
				resize->bottom = resize->top + height;

				// se a coordenada saiu da tela
				if (resize->bottom > window->ScreenHeight())
				{
					// mant�m a janela dentro da tela
					resize->bottom = window->ScreenHeight();

					// calcula nova largura para manter o aspect ratio
					height = resize->bottom - resize->top;
					width = uint(height * window->FullAspectRatio());

					// ajusta largura de acordo com o lado
					if (wParam == WMSZ_LEFT || wParam == WMSZ_BOTTOMLEFT)
						resize->left = resize->right - width;
					else
						resize->right = resize->left + width;
				}
				break;

			case WMSZ_BOTTOM:
			case WMSZ_TOP:
				// calcula coordenada para manter o aspect ratio
				width = uint(height * window->FullAspectRatio());
				resize->right = resize->left + width;

				// se a coordenada saiu da tela
				if (resize->right > window->ScreenWidth())
				{
					// mant�m a janela dentro da tela
					resize->right = window->ScreenWidth();

					// calcula nova altura para manter o aspect ratio
					width = resize->right - resize->left;
					height = uint(width / window->FullAspectRatio());

					// ajusta altura de acordo com o lado
					if (wParam == WMSZ_TOP)
						resize->top = resize->bottom - height;
					else
						resize->bottom = resize->top + height;
				}
				break;
			}


		}

		window->FullResize(width, height);
		return TRUE;
	}
	}
	
	return CallWindowProc(Input::InputProc, hWnd, msg, wParam, lParam);
}

// -----------------------------------------------------------------------------
