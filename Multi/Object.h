/**********************************************************************************
// Object (Arquivo de Cabe�alho)
//
// Cria��o:     14 Out 2022
// Atualiza��o: 15 Set 2023
// Compilador:  Visual C++ 2022
//
// Descri��o:   Define armazenamento para objeto de uma cena
//
**********************************************************************************/

#ifndef DXUT_OBJECT_H_
#define DXUT_OBJECT_H_

#include "Types.h"
#include "Mesh.h"
#include <DirectXMath.h>
using DirectX::XMFLOAT4X4;

struct Object
{
	XMFLOAT4X4 world = {            // matriz de mundo
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f };

	uint cbIndex = -1;			    // �ndice para o constant buffer
	Mesh * mesh = nullptr;			// malha de v�rtices
	SubMesh submesh {};	            // informa��es da sub-malha
};

#endif
