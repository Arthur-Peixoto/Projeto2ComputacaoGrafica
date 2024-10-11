/**********************************************************************************
// Geometry (Arquivo de Cabe�alho)
//
// Cria��o:     03 Fev 2013
// Atualiza��o: 10 Set 2023
// Compilador:  Visual C++ 2022
//
// Descri��o:   Define v�rtices e �ndices para v�rias geometrias
//
**********************************************************************************/

#ifndef DXUT_GEOMETRY_H_
#define DXUT_GEOMETRY_H_

// -------------------------------------------------------------------------------

#include "Types.h"
#include <vector>
#include <DirectXMath.h>
#include <DirectXColors.h>
using namespace DirectX;
using std::vector;

// -------------------------------------------------------------------------------

struct Vertex
{
    XMFLOAT3 pos;
    XMFLOAT4 color;
    XMFLOAT3 normal;
};

// -------------------------------------------------------------------------------
// Geometry
// -------------------------------------------------------------------------------

struct Geometry
{
    vector<Vertex> vertices;                // v�rtices da geometria
    vector<uint>   indices;                 // �ndices da geometria

    void Subdivide();                       // subdivide tri�ngulos

    // m�todos inline
    const Vertex* VertexData() const        // retorna v�rtices da geometria
    { return vertices.data(); }
    
    const uint* IndexData() const           // retorna �ndices da geometria
    { return indices.data(); }
    
    uint VertexCount() const                // retorna n�mero de v�rtices
    { return uint(vertices.size()); }
    
    uint IndexCount() const                 // retorna n�mero de �ndices
    { return uint(indices.size()); }
};

// -------------------------------------------------------------------------------
// Box
// -------------------------------------------------------------------------------

struct Box : public Geometry
{
    Box(float width, float height, float depth);
};

// -------------------------------------------------------------------------------
// Cylinder
// -------------------------------------------------------------------------------

struct Cylinder : public Geometry
{
    Cylinder(float bottom, float top, float height, uint sliceCount, uint stackCount);
};

// -------------------------------------------------------------------------------
// Sphere
// -------------------------------------------------------------------------------

struct Sphere : public Geometry
{
    Sphere(float radius, uint sliceCount, uint stackCount);
};

// -------------------------------------------------------------------------------
// GeoSphere
// -------------------------------------------------------------------------------

struct GeoSphere : public Geometry
{
    GeoSphere(float radius, uint subdivisions);
};


// -------------------------------------------------------------------------------
// Grid
// -------------------------------------------------------------------------------

struct Grid : public Geometry
{
    Grid(float width, float depth, uint m, uint n);
};

// -------------------------------------------------------------------------------
// Quad
// -------------------------------------------------------------------------------

struct Quad : public Geometry
{
    Quad(float width, float height);
};

// -------------------------------------------------------------------------------

#endif
