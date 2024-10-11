/**********************************************************************************
// Multi (Código Fonte)
//
// Criação:     27 Abr 2016
// Atualização: 15 Set 2023
// Compilador:  Visual C++ 2022
//
// Descrição:   Constrói cena usando vários buffers, um por objeto
//
**********************************************************************************/

#include "DXUT.h"
#include <fstream>
#include <sstream>
#include <iostream>

using namespace std;

struct ObjData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

// ------------------------------------------------------------------------------
struct ObjectConstants
{
    XMFLOAT4X4 WorldViewProj =
    { 1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f };

    //XMFLOAT4 color;
};

// ------------------------------------------------------------------------------

class Multi : public App
{
private:
    ID3D12RootSignature* rootSignature = nullptr;
    ID3D12PipelineState* pipelineState = nullptr;
    vector<Object> scene;
    vector<Object> sceneTopEsq;
    vector<Object> sceneTopDir;
    vector<Object> sceneBaixEsq;
    vector<Object> lines;
    vector<Object> lines2;


    Timer timer;
    bool quadViewMode = false; // controlar o modo Quadview

    bool isObjectSelected = false; // Variável para rastrear se um objeto está selecionado
    bool increasingScale = true; // Indica se a escala está aumentando ou diminuindo na animação de pulsação

    XMFLOAT4X4 Identity = {};
    XMFLOAT4X4 View = {};
    XMFLOAT4X4 Proj = {};
    XMFLOAT4 currentColor = XMFLOAT4(DirectX::Colors::DimGray);

    D3D12_VIEWPORT viewFront, viewTop, viewRight, viewPerspective; // QuadView
    D3D12_VIEWPORT lineVview, lineHview; // QuadView

    Object lineV;
    Object lineH;

    Mesh* perspective = nullptr;
    Mesh* ortographic = nullptr;
    bool wireframeMode = true;
    int wireframeModes = 0;

    float theta = 0;
    float phi = 0;
    float radius = 0;
    float lastMousePosX = 0;
    float lastMousePosY = 0;

    // Variável global para rastrear o índice do objeto selecionado
    int selectedIndex = 0;

public:
    void Init();
    void Update();
    void Draw();
    void Finalize();
    Geometry LoadOBJ(const std::string& filename);
    void CalculateNormals(Geometry& objData);
    void BuildRootSignature();
    void BuildPipelineState();
    void BuildPipelineStateFront();
    void BuildPipelineStateNone();
};

// ------------------------------------------------------------------------------

Geometry Multi::LoadOBJ(const std::string& filename) {
    Geometry objData;
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Failed to open OBJ file: " << filename << std::endl;
        return objData;
    }

    std::string line;
    std::vector<XMFLOAT3> positions;
    std::vector<XMFLOAT3> normals;
    std::vector<XMFLOAT2> texCoords;  // Para armazenar coordenadas de textura, se houver

    bool hasNormals = false;

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "v") {
            // Vértices (posições)
            XMFLOAT3 position;
            iss >> position.x >> position.y >> position.z;
            positions.push_back(position);
        }
        else if (prefix == "vn") {
            // Normais
            XMFLOAT3 normal;
            iss >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
            hasNormals = true;
        }
        else if (prefix == "vt") {
            // Coordenadas de textura
            XMFLOAT2 texCoord;
            iss >> texCoord.x >> texCoord.y;
            texCoords.push_back(texCoord);
        }
        else if (prefix == "f") {
            // Faces (índices de vértices)
            uint32_t v[3], vt[3], vn[3];
            char slash;
            std::string faceStr;
            getline(iss, faceStr);
            std::istringstream faceStream(faceStr);

            // Diferentes tipos de faces (com e sem normais/coordenadas de textura)
            if (faceStr.find("//") != std::string::npos) {
                // Formato: v1//vn1 v2//vn2 v3//vn3
                faceStream >> v[0] >> slash >> slash >> vn[0]
                    >> v[1] >> slash >> slash >> vn[1]
                    >> v[2] >> slash >> slash >> vn[2];
            }
            else if (faceStr.find("/") != std::string::npos) {
                // Formato: v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3
                faceStream >> v[0] >> slash >> vt[0] >> slash >> vn[0]
                    >> v[1] >> slash >> vt[1] >> slash >> vn[1]
                    >> v[2] >> slash >> vt[2] >> slash >> vn[2];
            }
            else {
                // Formato: v1 v2 v3
                faceStream >> v[0] >> v[1] >> v[2];
            }

            objData.indices.push_back(v[0] - 1);
            objData.indices.push_back(v[1] - 1);
            objData.indices.push_back(v[2] - 1);
        }
    }

    // Processa os vértices e aplica normais se existirem
    for (size_t i = 0; i < positions.size(); ++i) {
        Vertex vertex;
        vertex.pos = positions[i];
        vertex.color = XMFLOAT4(DirectX::Colors::DimGray);

        if (hasNormals && i < normals.size()) {
            vertex.normal = normals[i];
        }
        else {
            // Caso não existam normais, atribuímos um valor padrão (0, 0, 0)
            vertex.normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
        }

        objData.vertices.push_back(vertex);
    }


    return objData;
}


void Multi::Init()
{
    graphics->ResetCommands();
    // -----------------------------
    // Parâmetros Iniciais da Câmera
    // -----------------------------

    // controla rotação da câmera
    theta = XM_PIDIV4;
    phi = 1.3f;
    radius = 5.0f;

    // pega última posição do mouse
    lastMousePosX = (float)input->MouseX();
    lastMousePosY = (float)input->MouseY();

    // inicializa as matrizes Identity e View para a identidade
    Identity = View = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f };

    // inicializa a matriz de projeção
    XMStoreFloat4x4(&Proj, XMMatrixPerspectiveFovLH(
        XMConvertToRadians(45.0f),
        window->AspectRatio(),
        1.0f, 100.0f));

    // Criação da Geometria padrão = grid

    Grid grid(3.0f, 3.0f, 20, 20);

    for (auto& v : grid.vertices) v.color = XMFLOAT4(DirectX::Colors::DimGray);

    // ---------------------------------------------------------------
    // Alocação e Cópia de Vertex, Index e Constant Buffers para a GPU
    // ---------------------------------------------------------------

    // grid
    Object gridObj;
    gridObj.mesh = new Mesh();
    gridObj.world = Identity;
    gridObj.mesh->VertexBuffer(grid.VertexData(), grid.VertexCount() * sizeof(Vertex), sizeof(Vertex));
    gridObj.mesh->IndexBuffer(grid.IndexData(), grid.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
    gridObj.mesh->ConstantBuffer(sizeof(ObjectConstants));
    gridObj.submesh.indexCount = grid.IndexCount();
    scene.push_back(gridObj);

    // grid top left
    Object gridObj1;
    gridObj1.mesh = new Mesh();
    gridObj1.world = Identity;
    gridObj1.mesh->VertexBuffer(grid.VertexData(), grid.VertexCount() * sizeof(Vertex), sizeof(Vertex));
    gridObj1.mesh->IndexBuffer(grid.IndexData(), grid.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
    gridObj1.mesh->ConstantBuffer(sizeof(ObjectConstants));
    gridObj1.submesh.indexCount = grid.IndexCount();
    sceneTopEsq.push_back(gridObj1);

    // grid top righ
    Object gridObj2;
    gridObj2.mesh = new Mesh();
    gridObj2.world = Identity;
    gridObj2.mesh->VertexBuffer(grid.VertexData(), grid.VertexCount() * sizeof(Vertex), sizeof(Vertex));
    gridObj2.mesh->IndexBuffer(grid.IndexData(), grid.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
    gridObj2.mesh->ConstantBuffer(sizeof(ObjectConstants));
    gridObj2.submesh.indexCount = grid.IndexCount();
    sceneTopDir.push_back(gridObj2);

    // grid
    Object gridObj3;
    gridObj3.mesh = new Mesh();
    gridObj3.world = Identity;
    gridObj3.mesh->VertexBuffer(grid.VertexData(), grid.VertexCount() * sizeof(Vertex), sizeof(Vertex));
    gridObj3.mesh->IndexBuffer(grid.IndexData(), grid.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
    gridObj3.mesh->ConstantBuffer(sizeof(ObjectConstants));
    gridObj3.submesh.indexCount = grid.IndexCount();
    sceneBaixEsq.push_back(gridObj3);

    // Configuração da viewport para a visualização frontal => topo esquerda 
    viewFront.TopLeftX = 0.0f;
    viewFront.TopLeftY = 0.0f;
    viewFront.Width = float(window->Width() / 2);
    viewFront.Height = float(window->Height() / 2);
    viewFront.MinDepth = 0.0f;
    viewFront.MaxDepth = 1.0f;

    // Configuração da viewport para a visualização superior => esquerda abaixo
    viewTop.TopLeftX = 0.0f;
    viewTop.TopLeftY = float(window->Height() / 2);
    viewTop.Width = float(window->Width() / 2);
    viewTop.Height = float(window->Height() / 2);
    viewTop.MinDepth = 0.0f;
    viewTop.MaxDepth = 1.0f;

    // Configuração da viewport para a visualização da direita => topo direita
    viewRight.TopLeftX = float(window->Width() / 2);
    viewRight.TopLeftY = 0.0f;
    viewRight.Width = float(window->Width() / 2);
    viewRight.Height = float(window->Height() / 2);
    viewRight.MinDepth = 0.0f;
    viewRight.MaxDepth = 1.0f;

    // Configuração da viewport para a visualização de perspectiva => direita abaixo
    viewPerspective.TopLeftX = float(window->Width() / 2);
    viewPerspective.TopLeftY = float(window->Height() / 2);
    viewPerspective.Width = float(window->Width() / 2);
    viewPerspective.Height = float(window->Height() / 2);
    viewPerspective.MinDepth = 0.0f;
    viewPerspective.MaxDepth = 1.0f;


    lineVview.TopLeftX = float(window->Width() / 2); // Define a coordenada X no meio da tela
    lineVview.TopLeftY = 0.0f; // Define a coordenada Y inicial (neste exemplo, 0)
    lineVview.Width = 1.0f; // Define a largura como 1 (1 pixel de largura)
    lineVview.Height = float(window->Height()); // Define a altura para cobrir a altura inteira da tela
    lineVview.MinDepth = 0.0f; // Profundidade mínima
    lineVview.MaxDepth = 1.0f; // Profundidade máxima

    lineHview.TopLeftX = 0.0f; // Define a coordenada X inicial (neste exemplo, 0)
    lineHview.TopLeftY = float(window->Height() / 2); // Define a coordenada Y no meio da tela
    lineHview.Width = float(window->Width()); // Define a largura para cobrir a largura inteira da tela
    lineHview.Height = 1.0f; // Define a altura como 1 (1 pixel de altura)
    lineHview.MinDepth = 0.0f; // Profundidade mínima
    lineHview.MaxDepth = 1.0f; // Profundidade máxima


    // ---------------------------------------

    BuildRootSignature();
    BuildPipelineState();

    // ---------------------------------------
    graphics->SubmitCommands();

    timer.Start();
}

// ------------------------------------------------------------------------------

void Multi::Update()
{
    // sai com o pressionamento da tecla ESC
    if (input->KeyPress(VK_ESCAPE)) {
        window->Close();
    }


    /*  
    if (input->KeyPress('S'))
    {
        spinning = !spinning;

        if (spinning)
            timer.Start();
        else
            timer.Stop();
    }
    */

    //Quad
    if (input->KeyPress('Q'))
    {
        graphics->ResetCommands();
        Quad quad(2.0f, 2.0f);

        for (auto& v : quad.vertices) v.color = currentColor;
        Object quadObj;

        //posições do init
        XMStoreFloat4x4(&quadObj.world,
            XMMatrixScaling(0.4f, 0.4f, 0.4f) *
            XMMatrixTranslation(0.0f, 0.5f, 0.0f));

        quadObj.mesh = new Mesh();
        quadObj.mesh->VertexBuffer(quad.VertexData(), quad.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        quadObj.mesh->IndexBuffer(quad.IndexData(), quad.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        quadObj.mesh->ConstantBuffer(sizeof(ObjectConstants));
        quadObj.submesh.indexCount = quad.IndexCount();
        scene.push_back(quadObj);

        Object quadObj2;

        //posições do init
        XMStoreFloat4x4(&quadObj2.world,
            XMMatrixScaling(0.4f, 0.4f, 0.4f) *
            XMMatrixTranslation(0.0f, 0.5f, 0.0f));

        quadObj2.mesh = new Mesh();
        quadObj2.mesh->VertexBuffer(quad.VertexData(), quad.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        quadObj2.mesh->IndexBuffer(quad.IndexData(), quad.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        quadObj2.mesh->ConstantBuffer(sizeof(ObjectConstants));
        quadObj2.submesh.indexCount = quad.IndexCount();
        sceneBaixEsq.push_back(quadObj2);

        Object quadObj3;

        //posições do init
        XMStoreFloat4x4(&quadObj3.world,
            XMMatrixScaling(0.4f, 0.4f, 0.4f) *
            XMMatrixTranslation(0.0f, 0.5f, 0.0f));

        quadObj3.mesh = new Mesh();
        quadObj3.mesh->VertexBuffer(quad.VertexData(), quad.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        quadObj3.mesh->IndexBuffer(quad.IndexData(), quad.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        quadObj3.mesh->ConstantBuffer(sizeof(ObjectConstants));
        quadObj3.submesh.indexCount = quad.IndexCount();
        sceneTopEsq.push_back(quadObj3);

        Object quadObj4;

        //posições do init
        XMStoreFloat4x4(&quadObj4.world,
            XMMatrixScaling(0.4f, 0.4f, 0.4f) *
            XMMatrixTranslation(0.0f, 0.5f, 0.0f));
        quadObj4.mesh = new Mesh();
        quadObj4.mesh->VertexBuffer(quad.VertexData(), quad.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        quadObj4.mesh->IndexBuffer(quad.IndexData(), quad.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        quadObj4.mesh->ConstantBuffer(sizeof(ObjectConstants));
        quadObj4.submesh.indexCount = quad.IndexCount();
        sceneTopDir.push_back(quadObj4);

        BuildRootSignature();
        BuildPipelineState();

        graphics->SubmitCommands();

    }

    // Box
    if (input->KeyPress('B')) {
        graphics->ResetCommands();

        Box box(2.0f, 2.0f, 2.0f);
        for (auto& v : box.vertices) v.color = XMFLOAT4(DirectX::Colors::DimGray);
        Object boxObj;

        //posições do init
        //XMStoreFloat4x4(&boxObj.world,
        //XMMatrixScaling(0.4f, 0.4f, 0.4f)*
          //  XMMatrixTranslation(-1.0f, 0.41f, 1.0f));

        XMStoreFloat4x4(&boxObj.world,
            XMMatrixScaling(0.4f, 0.4f, 0.4f) *
            XMMatrixTranslation(0.0f, 0.5f, 0.0f));


        boxObj.mesh = new Mesh();
        boxObj.mesh->VertexBuffer(box.VertexData(), box.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        boxObj.mesh->IndexBuffer(box.IndexData(), box.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        boxObj.mesh->ConstantBuffer(sizeof(ObjectConstants));
        boxObj.submesh.indexCount = box.IndexCount();
        scene.push_back(boxObj);

        Object boxObj2;
        // colocar isso aqui vindo do mouse
        XMStoreFloat4x4(&boxObj2.world,
            XMMatrixScaling(0.4f, 0.4f, 0.4f) *
            XMMatrixTranslation(0.0f, 0.5f, 0.0f));

        boxObj2.mesh = new Mesh();
        boxObj2.mesh->VertexBuffer(box.VertexData(), box.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        boxObj2.mesh->IndexBuffer(box.IndexData(), box.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        boxObj2.mesh->ConstantBuffer(sizeof(ObjectConstants));
        boxObj2.submesh.indexCount = box.IndexCount();
        sceneBaixEsq.push_back(boxObj2);

        Object boxObj3;
        // colocar isso aqui vindo do mouse
        XMStoreFloat4x4(&boxObj3.world,
            XMMatrixScaling(0.4f, 0.4f, 0.4f) *
            XMMatrixTranslation(0.0f, 0.5f, 0.0f));

        boxObj3.mesh = new Mesh();
        boxObj3.mesh->VertexBuffer(box.VertexData(), box.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        boxObj3.mesh->IndexBuffer(box.IndexData(), box.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        boxObj3.mesh->ConstantBuffer(sizeof(ObjectConstants));
        boxObj3.submesh.indexCount = box.IndexCount();
        sceneTopDir.push_back(boxObj3);

        Object boxObj4;
        // colocar isso aqui vindo do mouse
        XMStoreFloat4x4(&boxObj4.world,
            XMMatrixScaling(0.4f, 0.4f, 0.4f) *
            XMMatrixTranslation(0.0f, 0.5f, 0.0f));

        boxObj4.mesh = new Mesh();
        boxObj4.mesh->VertexBuffer(box.VertexData(), box.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        boxObj4.mesh->IndexBuffer(box.IndexData(), box.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        boxObj4.mesh->ConstantBuffer(sizeof(ObjectConstants));
        boxObj4.submesh.indexCount = box.IndexCount();
        sceneTopEsq.push_back(boxObj4);

        BuildRootSignature();
        BuildPipelineState();

        graphics->SubmitCommands();
    }

    // Cilindro
    if (input->KeyPress('C')) {
        graphics->ResetCommands();

        Cylinder cylinder(1.0f, 0.5f, 3.0f, 20, 20);
        for (auto& v : cylinder.vertices) v.color = XMFLOAT4(DirectX::Colors::DimGray);
        Object cylinderObj;

        // do init
        //XMStoreFloat4x4(&cylinderObj.world,
          //  XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            //XMMatrixTranslation(1.0f, 0.75f, -1.0f));

        // cylinder
        XMStoreFloat4x4(&cylinderObj.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) * // 0,5 , 0,5 , 0,5
            XMMatrixTranslation(0.0f, 0.5f, 0.0f));

        cylinderObj.mesh = new Mesh();
        cylinderObj.mesh->VertexBuffer(cylinder.VertexData(), cylinder.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        cylinderObj.mesh->IndexBuffer(cylinder.IndexData(), cylinder.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        cylinderObj.mesh->ConstantBuffer(sizeof(ObjectConstants));
        cylinderObj.submesh.indexCount = cylinder.IndexCount();
        scene.push_back(cylinderObj);

        Object cylinderObj2;

        // cylinder
        XMStoreFloat4x4(&cylinderObj2.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) * // 0,5 , 0,5 , 0,5
            XMMatrixTranslation(0.0f, 0.5f, 0.0f));

        cylinderObj2.mesh = new Mesh();
        cylinderObj2.mesh->VertexBuffer(cylinder.VertexData(), cylinder.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        cylinderObj2.mesh->IndexBuffer(cylinder.IndexData(), cylinder.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        cylinderObj2.mesh->ConstantBuffer(sizeof(ObjectConstants));
        cylinderObj2.submesh.indexCount = cylinder.IndexCount();
        sceneBaixEsq.push_back(cylinderObj2);

        Object cylinderObj3;

        // cylinder
        XMStoreFloat4x4(&cylinderObj3.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) * // 0,5 , 0,5 , 0,5
            XMMatrixTranslation(0.0f, 0.5f, 0.0f));

        cylinderObj3.mesh = new Mesh();
        cylinderObj3.mesh->VertexBuffer(cylinder.VertexData(), cylinder.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        cylinderObj3.mesh->IndexBuffer(cylinder.IndexData(), cylinder.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        cylinderObj3.mesh->ConstantBuffer(sizeof(ObjectConstants));
        cylinderObj3.submesh.indexCount = cylinder.IndexCount();
        sceneTopEsq.push_back(cylinderObj3);

        Object cylinderObj4;

        // cylinder
        XMStoreFloat4x4(&cylinderObj4.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) * // 0,5 , 0,5 , 0,5
            XMMatrixTranslation(0.0f, 0.5f, 0.0f));

        cylinderObj4.mesh = new Mesh();
        cylinderObj4.mesh->VertexBuffer(cylinder.VertexData(), cylinder.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        cylinderObj4.mesh->IndexBuffer(cylinder.IndexData(), cylinder.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        cylinderObj4.mesh->ConstantBuffer(sizeof(ObjectConstants));
        cylinderObj4.submesh.indexCount = cylinder.IndexCount();
        sceneTopDir.push_back(cylinderObj4);

        BuildRootSignature();
        BuildPipelineState();

        graphics->SubmitCommands();
    }

    // Esfera
    if (input->KeyPress('S')) {
        graphics->ResetCommands();

        Sphere sphere(1.0f, 20, 20);
        for (auto& v : sphere.vertices) v.color = XMFLOAT4(DirectX::Colors::DimGray);
        Object sphereObj;

        //do init
        /*XMStoreFloat4x4(&sphereObj.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.5f, 0.0f));*/

            // sphere
        XMStoreFloat4x4(&sphereObj.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.5f, 0.0f));

        sphereObj.mesh = new Mesh();
        sphereObj.mesh->VertexBuffer(sphere.VertexData(), sphere.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        sphereObj.mesh->IndexBuffer(sphere.IndexData(), sphere.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        sphereObj.mesh->ConstantBuffer(sizeof(ObjectConstants));
        sphereObj.submesh.indexCount = sphere.IndexCount();
        scene.push_back(sphereObj);

        Object sphereObj2;
        // sphere
        XMStoreFloat4x4(&sphereObj2.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.5f, 0.0f));

        sphereObj2.mesh = new Mesh();
        sphereObj2.mesh->VertexBuffer(sphere.VertexData(), sphere.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        sphereObj2.mesh->IndexBuffer(sphere.IndexData(), sphere.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        sphereObj2.mesh->ConstantBuffer(sizeof(ObjectConstants));
        sphereObj2.submesh.indexCount = sphere.IndexCount();
        sceneBaixEsq.push_back(sphereObj2);

        Object sphereObj3;
        // sphere
        XMStoreFloat4x4(&sphereObj3.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.5f, 0.0f));

        sphereObj3.mesh = new Mesh();
        sphereObj3.mesh->VertexBuffer(sphere.VertexData(), sphere.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        sphereObj3.mesh->IndexBuffer(sphere.IndexData(), sphere.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        sphereObj3.mesh->ConstantBuffer(sizeof(ObjectConstants));
        sphereObj3.submesh.indexCount = sphere.IndexCount();
        sceneTopEsq.push_back(sphereObj3);

        Object sphereObj4;
        // sphere
        XMStoreFloat4x4(&sphereObj4.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.5f, 0.0f));

        sphereObj4.mesh = new Mesh();
        sphereObj4.mesh->VertexBuffer(sphere.VertexData(), sphere.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        sphereObj4.mesh->IndexBuffer(sphere.IndexData(), sphere.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        sphereObj4.mesh->ConstantBuffer(sizeof(ObjectConstants));
        sphereObj4.submesh.indexCount = sphere.IndexCount();
        sceneTopDir.push_back(sphereObj4);

        BuildRootSignature();
        BuildPipelineState();

        graphics->SubmitCommands();
    }

    // Globo
    if (input->KeyPress('G')) {
        graphics->ResetCommands();

        GeoSphere geoSphere(1.0f, 2);
        for (auto& v : geoSphere.vertices) v.color = XMFLOAT4(DirectX::Colors::White);
        Object geoSphereObj;

        // sphere
        XMStoreFloat4x4(&geoSphereObj.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.5f, 0.0f));

        geoSphereObj.mesh = new Mesh();
        geoSphereObj.mesh->VertexBuffer(geoSphere.VertexData(), geoSphere.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        geoSphereObj.mesh->IndexBuffer(geoSphere.IndexData(), geoSphere.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        geoSphereObj.mesh->ConstantBuffer(sizeof(ObjectConstants));
        geoSphereObj.submesh.indexCount = geoSphere.IndexCount();
        scene.push_back(geoSphereObj);

        Object geoSphereObj2;

        // sphere
        XMStoreFloat4x4(&geoSphereObj2.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.5f, 0.0f));

        geoSphereObj2.mesh = new Mesh();
        geoSphereObj2.mesh->VertexBuffer(geoSphere.VertexData(), geoSphere.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        geoSphereObj2.mesh->IndexBuffer(geoSphere.IndexData(), geoSphere.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        geoSphereObj2.mesh->ConstantBuffer(sizeof(ObjectConstants));
        geoSphereObj2.submesh.indexCount = geoSphere.IndexCount();
        sceneBaixEsq.push_back(geoSphereObj2);


        Object geoSphereObj3;

        // sphere
        XMStoreFloat4x4(&geoSphereObj3.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.5f, 0.0f));

        geoSphereObj3.mesh = new Mesh();
        geoSphereObj3.mesh->VertexBuffer(geoSphere.VertexData(), geoSphere.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        geoSphereObj3.mesh->IndexBuffer(geoSphere.IndexData(), geoSphere.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        geoSphereObj3.mesh->ConstantBuffer(sizeof(ObjectConstants));
        geoSphereObj3.submesh.indexCount = geoSphere.IndexCount();
        sceneTopEsq.push_back(geoSphereObj3);


        Object geoSphereObj4;

        // sphere
        XMStoreFloat4x4(&geoSphereObj4.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.5f, 0.0f));

        geoSphereObj4.mesh = new Mesh();
        geoSphereObj4.mesh->VertexBuffer(geoSphere.VertexData(), geoSphere.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        geoSphereObj4.mesh->IndexBuffer(geoSphere.IndexData(), geoSphere.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        geoSphereObj4.mesh->ConstantBuffer(sizeof(ObjectConstants));
        geoSphereObj4.submesh.indexCount = geoSphere.IndexCount();
        sceneTopDir.push_back(geoSphereObj4);


        BuildRootSignature();
        BuildPipelineState();

        graphics->SubmitCommands();
    }

    // Plano
    if (input->KeyPress('P')) {
        graphics->ResetCommands();

        Grid grid(5.0f, 3.0f, 20, 20);

        for (auto& v : grid.vertices) v.color = XMFLOAT4(DirectX::Colors::DimGray);
        Object gridObj;

        // grid
        gridObj.mesh = new Mesh();
        gridObj.world = Identity;
        gridObj.mesh->VertexBuffer(grid.VertexData(), grid.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        gridObj.mesh->IndexBuffer(grid.IndexData(), grid.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        gridObj.mesh->ConstantBuffer(sizeof(ObjectConstants));
        gridObj.submesh.indexCount = grid.IndexCount();
        scene.push_back(gridObj);

        Object gridObj2;

        // grid
        gridObj2.mesh = new Mesh();
        gridObj2.world = Identity;
        gridObj2.mesh->VertexBuffer(grid.VertexData(), grid.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        gridObj2.mesh->IndexBuffer(grid.IndexData(), grid.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        gridObj2.mesh->ConstantBuffer(sizeof(ObjectConstants));
        gridObj2.submesh.indexCount = grid.IndexCount();
        sceneBaixEsq.push_back(gridObj2);

        Object gridObj3;

        // grid
        gridObj3.mesh = new Mesh();
        gridObj3.world = Identity;
        gridObj3.mesh->VertexBuffer(grid.VertexData(), grid.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        gridObj3.mesh->IndexBuffer(grid.IndexData(), grid.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        gridObj3.mesh->ConstantBuffer(sizeof(ObjectConstants));
        gridObj3.submesh.indexCount = grid.IndexCount();
        sceneTopEsq.push_back(gridObj3);

        Object gridOb4;

        // grid
        gridOb4.mesh = new Mesh();
        gridOb4.world = Identity;
        gridOb4.mesh->VertexBuffer(grid.VertexData(), grid.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        gridOb4.mesh->IndexBuffer(grid.IndexData(), grid.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        gridOb4.mesh->ConstantBuffer(sizeof(ObjectConstants));
        gridOb4.submesh.indexCount = grid.IndexCount();
        sceneTopDir.push_back(gridOb4);
        sceneTopDir.push_back(gridOb4);

        BuildRootSignature();
        BuildPipelineState();

        graphics->SubmitCommands();
    }

    else if (input->KeyPress('1')) {
        OutputDebugString("Ball\n");

        // Carregar o arquivo .obj
        Geometry ballData = LoadOBJ("ball.obj");

        graphics->ResetCommands();

        Object obj; //Objeto
        XMStoreFloat4x4(&obj.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.0f, 0.0f));

        obj.mesh = new Mesh();
        obj.mesh->VertexBuffer(ballData.VertexData(), ballData.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        obj.mesh->IndexBuffer(ballData.IndexData(), ballData.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        obj.mesh->ConstantBuffer(sizeof(ObjectConstants));
        obj.submesh.indexCount = ballData.IndexCount();
        scene.push_back(obj);

        Object obj2; //Objeto
        XMStoreFloat4x4(&obj2.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.0f, 0.0f));

        obj2.mesh = new Mesh();
        obj2.mesh->VertexBuffer(ballData.VertexData(), ballData.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        obj2.mesh->IndexBuffer(ballData.IndexData(), ballData.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        obj2.mesh->ConstantBuffer(sizeof(ObjectConstants));
        obj2.submesh.indexCount = ballData.IndexCount();
        sceneBaixEsq.push_back(obj2);

        Object obj3; //Objeto
        XMStoreFloat4x4(&obj3.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.0f, 0.0f));

        obj3.mesh = new Mesh();
        obj3.mesh->VertexBuffer(ballData.VertexData(), ballData.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        obj3.mesh->IndexBuffer(ballData.IndexData(), ballData.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        obj3.mesh->ConstantBuffer(sizeof(ObjectConstants));
        obj3.submesh.indexCount = ballData.IndexCount();
        sceneTopEsq.push_back(obj3);

        Object obj4; //Objeto
        XMStoreFloat4x4(&obj4.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.0f, 0.0f));

        obj4.mesh = new Mesh();
        obj4.mesh->VertexBuffer(ballData.VertexData(), ballData.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        obj4.mesh->IndexBuffer(ballData.IndexData(), ballData.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        obj4.mesh->ConstantBuffer(sizeof(ObjectConstants));
        obj4.submesh.indexCount = ballData.IndexCount();
        sceneTopDir.push_back(obj4);

        graphics->SubmitCommands();



    }
    else if (input->KeyPress('2')) {
        OutputDebugString("Capsule\n");

        // Carregar o arquivo .obj
        Geometry capsuleData = LoadOBJ("capsule.obj");

        graphics->ResetCommands();

        Object obj; //Objeto
        XMStoreFloat4x4(&obj.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.0f, 0.0f));

        obj.mesh = new Mesh();
        obj.mesh->VertexBuffer(capsuleData.VertexData(), capsuleData.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        obj.mesh->IndexBuffer(capsuleData.IndexData(), capsuleData.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        obj.mesh->ConstantBuffer(sizeof(ObjectConstants));
        obj.submesh.indexCount = capsuleData.IndexCount();
        scene.push_back(obj);

        Object obj2; //Objeto

        XMStoreFloat4x4(&obj2.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.0f, 0.0f));

        obj2.mesh = new Mesh();
        obj2.mesh->VertexBuffer(capsuleData.VertexData(), capsuleData.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        obj2.mesh->IndexBuffer(capsuleData.IndexData(), capsuleData.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        obj2.mesh->ConstantBuffer(sizeof(ObjectConstants));
        obj2.submesh.indexCount = capsuleData.IndexCount();
        sceneBaixEsq.push_back(obj2);

        Object obj3; //Objeto

        XMStoreFloat4x4(&obj3.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.0f, 0.0f));

        obj3.mesh = new Mesh();
        obj3.mesh->VertexBuffer(capsuleData.VertexData(), capsuleData.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        obj3.mesh->IndexBuffer(capsuleData.IndexData(), capsuleData.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        obj3.mesh->ConstantBuffer(sizeof(ObjectConstants));
        obj3.submesh.indexCount = capsuleData.IndexCount();
        sceneTopEsq.push_back(obj3);

        Object obj4; //Objeto

        XMStoreFloat4x4(&obj4.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.0f, 0.0f));

        obj4.mesh = new Mesh();
        obj4.mesh->VertexBuffer(capsuleData.VertexData(), capsuleData.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        obj4.mesh->IndexBuffer(capsuleData.IndexData(), capsuleData.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        obj4.mesh->ConstantBuffer(sizeof(ObjectConstants));
        obj4.submesh.indexCount = capsuleData.IndexCount();
        sceneTopDir.push_back(obj4);

        graphics->SubmitCommands();
    }
    else if (input->KeyPress('3')) {
        OutputDebugString("House\n");

        // Carregar o arquivo .obj
        Geometry houseData = LoadOBJ("house.obj");

        graphics->ResetCommands();

        Object obj; //Objeto
        XMStoreFloat4x4(&obj.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.0f, 0.0f));

        obj.mesh = new Mesh();
        obj.mesh->VertexBuffer(houseData.VertexData(), houseData.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        obj.mesh->IndexBuffer(houseData.IndexData(), houseData.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        obj.mesh->ConstantBuffer(sizeof(ObjectConstants));
        obj.submesh.indexCount = houseData.IndexCount();
        scene.push_back(obj);

        Object obj2; //Objeto
        XMStoreFloat4x4(&obj2.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.0f, 0.0f));

        obj2.mesh = new Mesh();
        obj2.mesh->VertexBuffer(houseData.VertexData(), houseData.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        obj2.mesh->IndexBuffer(houseData.IndexData(), houseData.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        obj2.mesh->ConstantBuffer(sizeof(ObjectConstants));
        obj2.submesh.indexCount = houseData.IndexCount();
        sceneBaixEsq.push_back(obj2);

        Object obj3; //Objeto
        XMStoreFloat4x4(&obj3.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.0f, 0.0f));

        obj3.mesh = new Mesh();
        obj3.mesh->VertexBuffer(houseData.VertexData(), houseData.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        obj3.mesh->IndexBuffer(houseData.IndexData(), houseData.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        obj3.mesh->ConstantBuffer(sizeof(ObjectConstants));
        obj3.submesh.indexCount = houseData.IndexCount();
        sceneTopEsq.push_back(obj3);

        Object obj4; //Objeto
        XMStoreFloat4x4(&obj4.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.0f, 0.0f));

        obj4.mesh = new Mesh();
        obj4.mesh->VertexBuffer(houseData.VertexData(), houseData.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        obj4.mesh->IndexBuffer(houseData.IndexData(), houseData.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        obj4.mesh->ConstantBuffer(sizeof(ObjectConstants));
        obj4.submesh.indexCount = houseData.IndexCount();
        sceneTopDir.push_back(obj4);

        graphics->SubmitCommands();
    }
    else if (input->KeyPress('4')) {
        OutputDebugString("Monkey\n");

        // Carregar o arquivo .obj
        Geometry monkeyData = LoadOBJ("monkey.obj");

        graphics->ResetCommands();

        Object obj; //Objeto
        XMStoreFloat4x4(&obj.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.0f, 0.0f));

        obj.mesh = new Mesh();
        obj.mesh->VertexBuffer(monkeyData.VertexData(), monkeyData.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        obj.mesh->IndexBuffer(monkeyData.IndexData(), monkeyData.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        obj.mesh->ConstantBuffer(sizeof(ObjectConstants));
        obj.submesh.indexCount = monkeyData.IndexCount();
        scene.push_back(obj);

        Object obj2; //Objeto

        XMStoreFloat4x4(&obj2.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.0f, 0.0f));

        obj2.mesh = new Mesh();
        obj2.mesh->VertexBuffer(monkeyData.VertexData(), monkeyData.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        obj2.mesh->IndexBuffer(monkeyData.IndexData(), monkeyData.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        obj2.mesh->ConstantBuffer(sizeof(ObjectConstants));
        obj2.submesh.indexCount = monkeyData.IndexCount();
        sceneBaixEsq.push_back(obj2);

        Object obj3; //Objeto

        XMStoreFloat4x4(&obj3.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.0f, 0.0f));

        obj3.mesh = new Mesh();
        obj3.mesh->VertexBuffer(monkeyData.VertexData(), monkeyData.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        obj3.mesh->IndexBuffer(monkeyData.IndexData(), monkeyData.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        obj3.mesh->ConstantBuffer(sizeof(ObjectConstants));
        obj3.submesh.indexCount = monkeyData.IndexCount();
        sceneTopEsq.push_back(obj3);

        Object obj4; //Objeto

        XMStoreFloat4x4(&obj4.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.0f, 0.0f));

        obj4.mesh = new Mesh();
        obj4.mesh->VertexBuffer(monkeyData.VertexData(), monkeyData.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        obj4.mesh->IndexBuffer(monkeyData.IndexData(), monkeyData.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        obj4.mesh->ConstantBuffer(sizeof(ObjectConstants));
        obj4.submesh.indexCount = monkeyData.IndexCount();
        sceneTopDir.push_back(obj4);

        graphics->SubmitCommands();
    }
    else if (input->KeyPress('5')) {
        OutputDebugString("Bleach\n");

        // Carregar o arquivo .obj
        Geometry bleachData = LoadOBJ("HollofiedIchigo.obj");

        graphics->ResetCommands();

        Object obj; //Objeto
        XMStoreFloat4x4(&obj.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.0f, 0.0f));

        obj.mesh = new Mesh();
        obj.mesh->VertexBuffer(bleachData.VertexData(), bleachData.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        obj.mesh->IndexBuffer(bleachData.IndexData(), bleachData.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        obj.mesh->ConstantBuffer(sizeof(ObjectConstants));
        obj.submesh.indexCount = bleachData.IndexCount();
        scene.push_back(obj);

        Object obj2; //Objeto

        XMStoreFloat4x4(&obj2.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.0f, 0.0f));

        obj2.mesh = new Mesh();

        obj2.mesh->VertexBuffer(bleachData.VertexData(), bleachData.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        obj2.mesh->IndexBuffer(bleachData.IndexData(), bleachData.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        obj2.mesh->ConstantBuffer(sizeof(ObjectConstants));
        obj2.submesh.indexCount = bleachData.IndexCount();
        sceneBaixEsq.push_back(obj2);

        Object obj3; //Objeto

        XMStoreFloat4x4(&obj3.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.0f, 0.0f));

        obj3.mesh = new Mesh();
        obj3.mesh->VertexBuffer(bleachData.VertexData(), bleachData.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        obj3.mesh->IndexBuffer(bleachData.IndexData(), bleachData.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        obj3.mesh->ConstantBuffer(sizeof(ObjectConstants));
        obj3.submesh.indexCount = bleachData.IndexCount();
        sceneTopEsq.push_back(obj3);

        Object obj4; //Objeto

        XMStoreFloat4x4(&obj4.world,
            XMMatrixScaling(0.5f, 0.5f, 0.5f) *
            XMMatrixTranslation(0.0f, 0.0f, 0.0f));

        obj4.mesh = new Mesh();
        obj4.mesh->VertexBuffer(bleachData.VertexData(), bleachData.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        obj4.mesh->IndexBuffer(bleachData.IndexData(), bleachData.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        obj4.mesh->ConstantBuffer(sizeof(ObjectConstants));
        obj4.submesh.indexCount = bleachData.IndexCount();
        sceneTopDir.push_back(obj4);

        graphics->SubmitCommands();
    }


    if (input->KeyPress(VK_TAB))
    {
        // Incrementar o índice com base na direção desejada (neste exemplo, é para a direita)
        selectedIndex++;

        // Certifique-se de que o índice esteja dentro dos limites do vetor
        if (selectedIndex >= scene.size())
        {
            selectedIndex = 0; // Volte para o início
            
        }

    }

    // No caso de exclusão (por exemplo, quando a tecla Delete é pressionada)
    if (input->KeyPress(VK_DELETE))
    {
        if (!scene.empty())
        {
            // Verifique se o índice ainda está dentro dos limites antes de excluir
            if (selectedIndex >= 0 && selectedIndex < scene.size())
            {
                // Deletar o objeto selecionado
                scene.erase(scene.begin() + selectedIndex);
            }
        }

        if (!sceneBaixEsq.empty())
        {
            // Verifique se o índice ainda está dentro dos limites antes de excluir
            if (selectedIndex >= 0 && selectedIndex < sceneBaixEsq.size())
            {
                // Deletar o objeto selecionado
                sceneBaixEsq.erase(sceneBaixEsq.begin() + selectedIndex);
            }
        }

        if (!sceneTopEsq.empty())
        {
            // Verifique se o índice ainda está dentro dos limites antes de excluir
            if (selectedIndex >= 0 && selectedIndex < sceneTopEsq.size())
            {
                // Deletar o objeto selecionado
                sceneTopEsq.erase(sceneTopEsq.begin() + selectedIndex);
            }
        }

        if (!sceneTopDir.empty())
        {
            // Verifique se o índice ainda está dentro dos limites antes de excluir
            if (selectedIndex >= 0 && selectedIndex < sceneTopDir.size())
            {
                // Deletar o objeto selecionado
                sceneTopDir.erase(sceneTopDir.begin() + selectedIndex);
            }
        }
    }

    // Verifique se a tecla V foi pressionada para alternar o modo QuadView
    if (input->KeyPress('V'))
    {
        quadViewMode = !quadViewMode;
    }

    // mover para baixo
    if (input->KeyPress(VK_DOWN))
    {
        if (!scene.empty() && selectedIndex >= 0 && selectedIndex < scene.size())
        {
            Object& selectedObject = scene[selectedIndex];  // Observe o uso de uma referência (&) aqui.

            // Mova o objeto para baixo (negativo na direção y)
            selectedObject.world._42 -= 0.2f;  // Reduza a posição y (vertical) em 0.2
        }

        if (!sceneBaixEsq.empty() && selectedIndex >= 0 && selectedIndex < sceneBaixEsq.size())
        {
            Object& selectedObject = sceneBaixEsq[selectedIndex];  // Observe o uso de uma referência (&) aqui.

            // Mova o objeto para baixo (negativo na direção y)
            selectedObject.world._42 -= 0.2f;  // Reduza a posição y (vertical) em 0.2
        }

        if (!sceneTopEsq.empty() && selectedIndex >= 0 && selectedIndex < sceneTopEsq.size())
        {
            Object& selectedObject = sceneTopEsq[selectedIndex];  // Observe o uso de uma referência (&) aqui.

            // Mova o objeto para baixo (negativo na direção y)
            selectedObject.world._42 -= 0.2f;  // Reduza a posição y (vertical) em 0.2
        }

        if (!sceneTopDir.empty() && selectedIndex >= 0 && selectedIndex < sceneTopDir.size())
        {
            Object& selectedObject = sceneTopDir[selectedIndex];  // Observe o uso de uma referência (&) aqui.

            // Mova o objeto para baixo (negativo na direção y)
            selectedObject.world._42 -= 0.2f;  // Reduza a posição y (vertical) em 0.2
        }
    }

    // mover para cima
    if (input->KeyPress(VK_UP))
    {
        if (!scene.empty() && selectedIndex >= 0 && selectedIndex < scene.size())
        {
            Object& selectedObject = scene[selectedIndex];  // Observe o uso de uma referência (&) aqui.

            // Mova o objeto para baixo (negativo na direção y)
            selectedObject.world._42 += 0.2f;  // Reduza a posição y (vertical) em 0.2
        }

        if (!sceneBaixEsq.empty() && selectedIndex >= 0 && selectedIndex < sceneBaixEsq.size())
        {
            Object& selectedObject = sceneBaixEsq[selectedIndex];  // Observe o uso de uma referência (&) aqui.

            // Mova o objeto para baixo (negativo na direção y)
            selectedObject.world._42 += 0.2f;  // Reduza a posição y (vertical) em 0.2
        }

        if (!sceneTopEsq.empty() && selectedIndex >= 0 && selectedIndex < sceneTopEsq.size())
        {
            Object& selectedObject = sceneTopEsq[selectedIndex];  // Observe o uso de uma referência (&) aqui.

            // Mova o objeto para baixo (negativo na direção y)
            selectedObject.world._42 += 0.2f;  // Reduza a posição y (vertical) em 0.2
        }

        if (!sceneTopDir.empty() && selectedIndex >= 0 && selectedIndex < sceneTopDir.size())
        {
            Object& selectedObject = sceneTopDir[selectedIndex];  // Observe o uso de uma referência (&) aqui.

            // Mova o objeto para baixo (negativo na direção y)
            selectedObject.world._42 += 0.2f;  // Reduza a posição y (vertical) em 0.2
        }
    }

    // mover para direita
    if (input->KeyPress(VK_RIGHT))
    {
        if (!scene.empty() && selectedIndex >= 0 && selectedIndex < scene.size())
        {
            Object& selectedObject = scene[selectedIndex];

            // Mova o objeto para a direita (positivo na direção x)
            selectedObject.world._41 += 0.2f;  // Aumente a posição x (horizontal) em 0.25f
        }

        if (!sceneBaixEsq.empty() && selectedIndex >= 0 && selectedIndex < sceneBaixEsq.size())
        {
            Object& selectedObject = sceneBaixEsq[selectedIndex];

            // Mova o objeto para a direita (positivo na direção x)
            selectedObject.world._41 += 0.2f;  // Aumente a posição x (horizontal) em 0.2f
        }

        if (!sceneTopEsq.empty() && selectedIndex >= 0 && selectedIndex < sceneTopEsq.size())
        {
            Object& selectedObject = sceneTopEsq[selectedIndex];

            // Mova o objeto para a direita (positivo na direção x)
            selectedObject.world._41 += 0.2f;  // Aumente a posição x (horizontal) em 0.2f
        }

        if (!sceneTopDir.empty() && selectedIndex >= 0 && selectedIndex < sceneTopDir.size())
        {
            Object& selectedObject = sceneTopDir[selectedIndex];

            // Mova o objeto para a direita (positivo na direção x)
            selectedObject.world._41 += 0.2f;  // Aumente a posição x (horizontal) em 0.2f
        }
    }

    // mover para esquerda
    if (input->KeyPress(VK_LEFT))
    {
        if (!scene.empty() && selectedIndex >= 0 && selectedIndex < scene.size())
        {
            Object& selectedObject = scene[selectedIndex];

            // Mova o objeto para a esquerda (negativo na direção x)
            selectedObject.world._41 -= 0.2f; // Reduza a posição x (horizontal) em 0.2f;
        }

        if (!sceneBaixEsq.empty() && selectedIndex >= 0 && selectedIndex < sceneBaixEsq.size())
        {
            Object& selectedObject = sceneBaixEsq[selectedIndex];

            // Mova o objeto para a esquerda (negativo na direção x)
            selectedObject.world._41 -= 0.2f; // Reduza a posição x (horizontal) em 0.2f;
        }

        if (!sceneTopEsq.empty() && selectedIndex >= 0 && selectedIndex < sceneTopEsq.size())
        {
            Object& selectedObject = sceneTopEsq[selectedIndex];

            // Mova o objeto para a esquerda (negativo na direção x)
            selectedObject.world._41 -= 0.2f; // Reduza a posição x (horizontal) em 0.2f;
        }

        if (!sceneTopDir.empty() && selectedIndex >= 0 && selectedIndex < sceneTopDir.size())
        {
            Object& selectedObject = sceneTopDir[selectedIndex];

            // Mova o objeto para a esquerda (negativo na direção x)
            selectedObject.world._41 -= 0.2f; // Reduza a posição x (horizontal) em 0.2f;
        }
    }

    // Rotacionar girando esquerda para direira
    if (input->KeyDown('J'))
    {
        if (!scene.empty() && selectedIndex >= 0 && selectedIndex < scene.size())
        {
            Object& selectedObject = scene[selectedIndex];

            // ângulo de rotação desejado em radianos
            float angleInRadians = XMConvertToRadians(-5.0f); // 5 graus

            XMMATRIX rotationMatrix = XMMatrixRotationY(angleInRadians);

            // Carregando a matriz de transformação atual em uma XMMATRIX
            XMMATRIX currentTransform = XMLoadFloat4x4(&selectedObject.world);

            // Multiplicando a matriz de rotação pela matriz de transformação atual
            XMMATRIX newTransform = rotationMatrix * currentTransform;

            // Armazenando a nova matriz de transformação no objeto
            XMStoreFloat4x4(&selectedObject.world, newTransform);
        }

        if (!sceneBaixEsq.empty() && selectedIndex >= 0 && selectedIndex < sceneBaixEsq.size())
        {
            Object& selectedObject = sceneBaixEsq[selectedIndex];

            // ângulo de rotação desejado em radianos
            float angleInRadians = XMConvertToRadians(-5.0f); // 5 graus

            XMMATRIX rotationMatrix = XMMatrixRotationY(angleInRadians);

            // Carregando a matriz de transformação atual em uma XMMATRIX
            XMMATRIX currentTransform = XMLoadFloat4x4(&selectedObject.world);

            // Multiplicando a matriz de rotação pela matriz de transformação atual
            XMMATRIX newTransform = rotationMatrix * currentTransform;

            // Armazenando a nova matriz de transformação no objeto
            XMStoreFloat4x4(&selectedObject.world, newTransform);
        }

        if (!sceneTopEsq.empty() && selectedIndex >= 0 && selectedIndex < sceneTopEsq.size())
        {
            Object& selectedObject = sceneTopEsq[selectedIndex];

            // ângulo de rotação desejado em radianos
            float angleInRadians = XMConvertToRadians(-5.0f); // 5 graus

            XMMATRIX rotationMatrix = XMMatrixRotationY(angleInRadians);

            // Carregando a matriz de transformação atual em uma XMMATRIX
            XMMATRIX currentTransform = XMLoadFloat4x4(&selectedObject.world);

            // Multiplicando a matriz de rotação pela matriz de transformação atual
            XMMATRIX newTransform = rotationMatrix * currentTransform;

            // Armazenando a nova matriz de transformação no objeto
            XMStoreFloat4x4(&selectedObject.world, newTransform);
        }

        if (!sceneTopDir.empty() && selectedIndex >= 0 && selectedIndex < sceneTopDir.size())
        {
            Object& selectedObject = sceneTopDir[selectedIndex];

            // ângulo de rotação desejado em radianos
            float angleInRadians = XMConvertToRadians(-5.0f); // 5 graus

            XMMATRIX rotationMatrix = XMMatrixRotationY(angleInRadians);

            // Carregando a matriz de transformação atual em uma XMMATRIX
            XMMATRIX currentTransform = XMLoadFloat4x4(&selectedObject.world);

            // Multiplicando a matriz de rotação pela matriz de transformação atual
            XMMATRIX newTransform = rotationMatrix * currentTransform;

            // Armazenando a nova matriz de transformação no objeto
            XMStoreFloat4x4(&selectedObject.world, newTransform);
        }

    }

    // Rotacionar girando direita para esquerda
    if (input->KeyDown('L'))
    {
        if (!scene.empty() && selectedIndex >= 0 && selectedIndex < scene.size())
        {
            Object& selectedObject = scene[selectedIndex];

            // ângulo de rotação desejado em radianos
            float angleInRadians = XMConvertToRadians(5.0f); // 5 graus

            XMMATRIX rotationMatrix = XMMatrixRotationY(angleInRadians);

            // Carregando a matriz de transformação atual em uma XMMATRIX
            XMMATRIX currentTransform = XMLoadFloat4x4(&selectedObject.world);

            // Multiplicando a matriz de rotação pela matriz de transformação atual
            XMMATRIX newTransform = rotationMatrix * currentTransform;

            // Armazenando a nova matriz de transformação no objeto
            XMStoreFloat4x4(&selectedObject.world, newTransform);
        }

        if (!sceneBaixEsq.empty() && selectedIndex >= 0 && selectedIndex < sceneBaixEsq.size())
        {
            Object& selectedObject = sceneBaixEsq[selectedIndex];

            // ângulo de rotação desejado em radianos
            float angleInRadians = XMConvertToRadians(5.0f); // 5 graus

            XMMATRIX rotationMatrix = XMMatrixRotationY(angleInRadians);

            // Carregando a matriz de transformação atual em uma XMMATRIX
            XMMATRIX currentTransform = XMLoadFloat4x4(&selectedObject.world);

            // Multiplicando a matriz de rotação pela matriz de transformação atual
            XMMATRIX newTransform = rotationMatrix * currentTransform;

            // Armazenando a nova matriz de transformação no objeto
            XMStoreFloat4x4(&selectedObject.world, newTransform);
        }

        if (!sceneTopEsq.empty() && selectedIndex >= 0 && selectedIndex < sceneTopEsq.size())
        {
            Object& selectedObject = sceneTopEsq[selectedIndex];

            // ângulo de rotação desejado em radianos
            float angleInRadians = XMConvertToRadians(5.0f); // 5 graus

            XMMATRIX rotationMatrix = XMMatrixRotationY(angleInRadians);

            // Carregando a matriz de transformação atual em uma XMMATRIX
            XMMATRIX currentTransform = XMLoadFloat4x4(&selectedObject.world);

            // Multiplicando a matriz de rotação pela matriz de transformação atual
            XMMATRIX newTransform = rotationMatrix * currentTransform;

            // Armazenando a nova matriz de transformação no objeto
            XMStoreFloat4x4(&selectedObject.world, newTransform);
        }

        if (!sceneTopDir.empty() && selectedIndex >= 0 && selectedIndex < sceneTopDir.size())
        {
            Object& selectedObject = sceneTopDir[selectedIndex];

            // ângulo de rotação desejado em radianos
            float angleInRadians = XMConvertToRadians(5.0f); // 5 graus

            XMMATRIX rotationMatrix = XMMatrixRotationY(angleInRadians);

            // Carregando a matriz de transformação atual em uma XMMATRIX
            XMMATRIX currentTransform = XMLoadFloat4x4(&selectedObject.world);

            // Multiplicando a matriz de rotação pela matriz de transformação atual
            XMMATRIX newTransform = rotationMatrix * currentTransform;

            // Armazenando a nova matriz de transformação no objeto
            XMStoreFloat4x4(&selectedObject.world, newTransform);
        }

    }

    // girando cima para baixo
    if (input->KeyDown('I'))
    {
        if (!scene.empty() && selectedIndex >= 0 && selectedIndex < scene.size())
        {
            Object& selectedObject = scene[selectedIndex];

            // Defina o ângulo de rotação desejado em radianos (sentido horário)
            float angleInRadians = XMConvertToRadians(5.0f); // Por exemplo, 5 graus no sentido horário

            // Crie uma matriz de rotação em torno do eixo X
            XMMATRIX rotationMatrix = XMMatrixRotationX(angleInRadians);

            // Carregue a matriz de transformação atual em uma XMMATRIX
            XMMATRIX currentTransform = XMLoadFloat4x4(&selectedObject.world);

            // Multiplique a matriz de rotação pela matriz de transformação atual
            XMMATRIX newTransform = rotationMatrix * currentTransform;

            // Armazene a nova matriz de transformação no objeto
            XMStoreFloat4x4(&selectedObject.world, newTransform);
        }

        if (!sceneBaixEsq.empty() && selectedIndex >= 0 && selectedIndex < sceneBaixEsq.size())
        {
            Object& selectedObject = sceneBaixEsq[selectedIndex];

            // Defina o ângulo de rotação desejado em radianos (sentido horário)
            float angleInRadians = XMConvertToRadians(5.0f); // Por exemplo, 5 graus no sentido horário

            // Crie uma matriz de rotação em torno do eixo X
            XMMATRIX rotationMatrix = XMMatrixRotationX(angleInRadians);

            // Carregue a matriz de transformação atual em uma XMMATRIX
            XMMATRIX currentTransform = XMLoadFloat4x4(&selectedObject.world);

            // Multiplique a matriz de rotação pela matriz de transformação atual
            XMMATRIX newTransform = rotationMatrix * currentTransform;

            // Armazene a nova matriz de transformação no objeto
            XMStoreFloat4x4(&selectedObject.world, newTransform);
        }

        if (!sceneTopEsq.empty() && selectedIndex >= 0 && selectedIndex < sceneTopEsq.size())
        {
            Object& selectedObject = sceneTopEsq[selectedIndex];

            // Defina o ângulo de rotação desejado em radianos (sentido horário)
            float angleInRadians = XMConvertToRadians(5.0f); // Por exemplo, 5 graus no sentido horário

            // Crie uma matriz de rotação em torno do eixo X
            XMMATRIX rotationMatrix = XMMatrixRotationX(angleInRadians);

            // Carregue a matriz de transformação atual em uma XMMATRIX
            XMMATRIX currentTransform = XMLoadFloat4x4(&selectedObject.world);

            // Multiplique a matriz de rotação pela matriz de transformação atual
            XMMATRIX newTransform = rotationMatrix * currentTransform;

            // Armazene a nova matriz de transformação no objeto
            XMStoreFloat4x4(&selectedObject.world, newTransform);
        }

        if (!sceneTopDir.empty() && selectedIndex >= 0 && selectedIndex < sceneTopDir.size())
        {
            Object& selectedObject = sceneTopDir[selectedIndex];

            // Defina o ângulo de rotação desejado em radianos (sentido horário)
            float angleInRadians = XMConvertToRadians(5.0f); // Por exemplo, 5 graus no sentido horário

            // Crie uma matriz de rotação em torno do eixo X
            XMMATRIX rotationMatrix = XMMatrixRotationX(angleInRadians);

            // Carregue a matriz de transformação atual em uma XMMATRIX
            XMMATRIX currentTransform = XMLoadFloat4x4(&selectedObject.world);

            // Multiplique a matriz de rotação pela matriz de transformação atual
            XMMATRIX newTransform = rotationMatrix * currentTransform;

            // Armazene a nova matriz de transformação no objeto
            XMStoreFloat4x4(&selectedObject.world, newTransform);
        }
    }

    // girando baixo para cima
    if (input->KeyDown('K'))
    {
        if (!scene.empty() && selectedIndex >= 0 && selectedIndex < scene.size())
        {
            Object& selectedObject = scene[selectedIndex];

            // Defina o ângulo de rotação desejado em radianos (sentido horário)
            float angleInRadians = XMConvertToRadians(-5.0f); // Por exemplo, 5 graus no sentido horário

            // Crie uma matriz de rotação em torno do eixo X
            XMMATRIX rotationMatrix = XMMatrixRotationX(angleInRadians);

            // Carregue a matriz de transformação atual em uma XMMATRIX
            XMMATRIX currentTransform = XMLoadFloat4x4(&selectedObject.world);

            // Multiplique a matriz de rotação pela matriz de transformação atual
            XMMATRIX newTransform = rotationMatrix * currentTransform;

            // Armazene a nova matriz de transformação no objeto
            XMStoreFloat4x4(&selectedObject.world, newTransform);
        }

        if (!sceneBaixEsq.empty() && selectedIndex >= 0 && selectedIndex < sceneBaixEsq.size())
        {
            Object& selectedObject = sceneBaixEsq[selectedIndex];

            // Defina o ângulo de rotação desejado em radianos (sentido horário)
            float angleInRadians = XMConvertToRadians(-5.0f); // Por exemplo, 5 graus no sentido horário

            // Crie uma matriz de rotação em torno do eixo X
            XMMATRIX rotationMatrix = XMMatrixRotationX(angleInRadians);

            // Carregue a matriz de transformação atual em uma XMMATRIX
            XMMATRIX currentTransform = XMLoadFloat4x4(&selectedObject.world);

            // Multiplique a matriz de rotação pela matriz de transformação atual
            XMMATRIX newTransform = rotationMatrix * currentTransform;

            // Armazene a nova matriz de transformação no objeto
            XMStoreFloat4x4(&selectedObject.world, newTransform);
        }

        if (!sceneTopEsq.empty() && selectedIndex >= 0 && selectedIndex < sceneTopEsq.size())
        {
            Object& selectedObject = sceneTopEsq[selectedIndex];

            // Defina o ângulo de rotação desejado em radianos (sentido horário)
            float angleInRadians = XMConvertToRadians(-5.0f); // Por exemplo, 5 graus no sentido horário

            // Crie uma matriz de rotação em torno do eixo X
            XMMATRIX rotationMatrix = XMMatrixRotationX(angleInRadians);

            // Carregue a matriz de transformação atual em uma XMMATRIX
            XMMATRIX currentTransform = XMLoadFloat4x4(&selectedObject.world);

            // Multiplique a matriz de rotação pela matriz de transformação atual
            XMMATRIX newTransform = rotationMatrix * currentTransform;

            // Armazene a nova matriz de transformação no objeto
            XMStoreFloat4x4(&selectedObject.world, newTransform);
        }

        if (!sceneTopDir.empty() && selectedIndex >= 0 && selectedIndex < sceneTopDir.size())
        {
            Object& selectedObject = sceneTopDir[selectedIndex];

            // Defina o ângulo de rotação desejado em radianos (sentido horário)
            float angleInRadians = XMConvertToRadians(-5.0f); // Por exemplo, 5 graus no sentido horário

            // Crie uma matriz de rotação em torno do eixo X
            XMMATRIX rotationMatrix = XMMatrixRotationX(angleInRadians);

            // Carregue a matriz de transformação atual em uma XMMATRIX
            XMMATRIX currentTransform = XMLoadFloat4x4(&selectedObject.world);

            // Multiplique a matriz de rotação pela matriz de transformação atual
            XMMATRIX newTransform = rotationMatrix * currentTransform;

            // Armazene a nova matriz de transformação no objeto
            XMStoreFloat4x4(&selectedObject.world, newTransform);
        }
    }

    // aumenta escala
    if (input->KeyDown('Y'))
    {
        if (!scene.empty() && selectedIndex >= 0 && selectedIndex < scene.size()) {
            Object& selectedObject = scene[selectedIndex];

            // Defina os fatores de escala (aumentar a escala em 20% no eixo Y)
            float scaleX = 1.0f + 0.25f;
            float scaleY = 1.0f + 0.25f;  // Aumentar a escala em scaleFactor no eixo Y
            float scaleZ = 1.0f + +0.25f;

            // Crie uma matriz de escala
            XMMATRIX scaleMatrix = XMMatrixScaling(scaleX, scaleY, scaleZ);

            // Carregue a matriz de transformação atual do objeto em uma XMMATRIX
            XMMATRIX currentTransform = XMLoadFloat4x4(&selectedObject.world);

            // Multiplique a matriz de escala pela matriz de transformação atual
            XMMATRIX newTransform = scaleMatrix * currentTransform;

            // Armazene a nova matriz de transformação no objeto
            XMStoreFloat4x4(&selectedObject.world, newTransform);
        }

        if (!sceneBaixEsq.empty() && selectedIndex >= 0 && selectedIndex < sceneBaixEsq.size()) {
            Object& selectedObject = sceneBaixEsq[selectedIndex];

            // Defina os fatores de escala (aumentar a escala em 20% no eixo Y)
            float scaleX = 1.0f + 0.25f;
            float scaleY = 1.0f + 0.25f;  // Aumentar a escala em scaleFactor no eixo Y
            float scaleZ = 1.0f + +0.25f;

            // Crie uma matriz de escala
            XMMATRIX scaleMatrix = XMMatrixScaling(scaleX, scaleY, scaleZ);

            // Carregue a matriz de transformação atual do objeto em uma XMMATRIX
            XMMATRIX currentTransform = XMLoadFloat4x4(&selectedObject.world);

            // Multiplique a matriz de escala pela matriz de transformação atual
            XMMATRIX newTransform = scaleMatrix * currentTransform;

            // Armazene a nova matriz de transformação no objeto
            XMStoreFloat4x4(&selectedObject.world, newTransform);
        }

        if (!sceneTopEsq.empty() && selectedIndex >= 0 && selectedIndex < sceneTopEsq.size()) {
            Object& selectedObject = sceneTopEsq[selectedIndex];

            // Defina os fatores de escala (aumentar a escala em 20% no eixo Y)
            float scaleX = 1.0f + 0.25f;
            float scaleY = 1.0f + 0.25f;  // Aumentar a escala em scaleFactor no eixo Y
            float scaleZ = 1.0f + +0.25f;

            // Crie uma matriz de escala
            XMMATRIX scaleMatrix = XMMatrixScaling(scaleX, scaleY, scaleZ);

            // Carregue a matriz de transformação atual do objeto em uma XMMATRIX
            XMMATRIX currentTransform = XMLoadFloat4x4(&selectedObject.world);

            // Multiplique a matriz de escala pela matriz de transformação atual
            XMMATRIX newTransform = scaleMatrix * currentTransform;

            // Armazene a nova matriz de transformação no objeto
            XMStoreFloat4x4(&selectedObject.world, newTransform);
        }

        if (!sceneTopDir.empty() && selectedIndex >= 0 && selectedIndex < sceneTopDir.size()) {
            Object& selectedObject = sceneTopDir[selectedIndex];

            // Defina os fatores de escala (aumentar a escala em 20% no eixo Y)
            float scaleX = 1.0f + 0.25f;
            float scaleY = 1.0f + 0.25f;  // Aumentar a escala em scaleFactor no eixo Y
            float scaleZ = 1.0f + +0.25f;

            // Crie uma matriz de escala
            XMMATRIX scaleMatrix = XMMatrixScaling(scaleX, scaleY, scaleZ);

            // Carregue a matriz de transformação atual do objeto em uma XMMATRIX
            XMMATRIX currentTransform = XMLoadFloat4x4(&selectedObject.world);

            // Multiplique a matriz de escala pela matriz de transformação atual
            XMMATRIX newTransform = scaleMatrix * currentTransform;

            // Armazene a nova matriz de transformação no objeto
            XMStoreFloat4x4(&selectedObject.world, newTransform);
        }
    }

    // diminui escala
    if (input->KeyDown('H'))
    {
        if (!scene.empty() && selectedIndex >= 0 && selectedIndex < scene.size()) {
            Object& selectedObject = scene[selectedIndex];

            // Defina os fatores de escala (aumentar a escala em 20% no eixo Y)
            float scaleX = 1.0f - 0.25f;
            float scaleY = 1.0f - 0.25f;  // Aumentar a escala em scaleFactor no eixo Y
            float scaleZ = 1.0f - 0.25f;

            // Crie uma matriz de escala
            XMMATRIX scaleMatrix = XMMatrixScaling(scaleX, scaleY, scaleZ);

            // Carregue a matriz de transformação atual do objeto em uma XMMATRIX
            XMMATRIX currentTransform = XMLoadFloat4x4(&selectedObject.world);

            // Multiplique a matriz de escala pela matriz de transformação atual
            XMMATRIX newTransform = scaleMatrix * currentTransform;

            // Armazene a nova matriz de transformação no objeto
            XMStoreFloat4x4(&selectedObject.world, newTransform);
        }

        if (!sceneBaixEsq.empty() && selectedIndex >= 0 && selectedIndex < sceneBaixEsq.size()) {
            Object& selectedObject = sceneBaixEsq[selectedIndex];

            // Defina os fatores de escala (aumentar a escala em 20% no eixo Y)
            float scaleX = 1.0f - 0.25f;
            float scaleY = 1.0f - 0.25f;  // Aumentar a escala em scaleFactor no eixo Y
            float scaleZ = 1.0f - 0.25f;

            // Crie uma matriz de escala
            XMMATRIX scaleMatrix = XMMatrixScaling(scaleX, scaleY, scaleZ);

            // Carregue a matriz de transformação atual do objeto em uma XMMATRIX
            XMMATRIX currentTransform = XMLoadFloat4x4(&selectedObject.world);

            // Multiplique a matriz de escala pela matriz de transformação atual
            XMMATRIX newTransform = scaleMatrix * currentTransform;

            // Armazene a nova matriz de transformação no objeto
            XMStoreFloat4x4(&selectedObject.world, newTransform);
        }

        if (!sceneTopEsq.empty() && selectedIndex >= 0 && selectedIndex < sceneTopEsq.size()) {
            Object& selectedObject = sceneTopEsq[selectedIndex];

            // Defina os fatores de escala (aumentar a escala em 20% no eixo Y)
            float scaleX = 1.0f - 0.25f;
            float scaleY = 1.0f - 0.25f;  // Aumentar a escala em scaleFactor no eixo Y
            float scaleZ = 1.0f - 0.25f;

            // Crie uma matriz de escala
            XMMATRIX scaleMatrix = XMMatrixScaling(scaleX, scaleY, scaleZ);

            // Carregue a matriz de transformação atual do objeto em uma XMMATRIX
            XMMATRIX currentTransform = XMLoadFloat4x4(&selectedObject.world);

            // Multiplique a matriz de escala pela matriz de transformação atual
            XMMATRIX newTransform = scaleMatrix * currentTransform;

            // Armazene a nova matriz de transformação no objeto
            XMStoreFloat4x4(&selectedObject.world, newTransform);
        }

        if (!sceneTopDir.empty() && selectedIndex >= 0 && selectedIndex < sceneTopDir.size()) {
            Object& selectedObject = sceneTopDir[selectedIndex];

            // Defina os fatores de escala (aumentar a escala em 20% no eixo Y)
            float scaleX = 1.0f - 0.25f;
            float scaleY = 1.0f - 0.25f;  // Aumentar a escala em scaleFactor no eixo Y
            float scaleZ = 1.0f - 0.25f;

            // Crie uma matriz de escala
            XMMATRIX scaleMatrix = XMMatrixScaling(scaleX, scaleY, scaleZ);

            // Carregue a matriz de transformação atual do objeto em uma XMMATRIX
            XMMATRIX currentTransform = XMLoadFloat4x4(&selectedObject.world);

            // Multiplique a matriz de escala pela matriz de transformação atual
            XMMATRIX newTransform = scaleMatrix * currentTransform;

            // Armazene a nova matriz de transformação no objeto
            XMStoreFloat4x4(&selectedObject.world, newTransform);
        }
    }

    // mouse positions
    float mousePosX = (float)input->MouseX();
    float mousePosY = (float)input->MouseY();

    if (input->KeyDown(VK_LBUTTON) && quadViewMode == false)
    {
        // cada pixel corresponde a 1/4 de grau
        float dx = XMConvertToRadians(0.25f * (mousePosX - lastMousePosX));
        float dy = XMConvertToRadians(0.25f * (mousePosY - lastMousePosY));

        // atualiza ângulos com base no deslocamento do mouse
        // para orbitar a câmera ao redor da caixa
        theta += dx;
        phi += dy;

        // restringe o ângulo de phi ]0-180[ graus
        phi = phi < 0.1f ? 0.1f : (phi > (XM_PI - 0.1f) ? XM_PI - 0.1f : phi);
    }
    else if (input->KeyDown(VK_RBUTTON) && quadViewMode == false)
    {
        // cada pixel corresponde a 0.05 unidades
        float dx = 0.05f * (mousePosX - lastMousePosX);
        float dy = 0.05f * (mousePosY - lastMousePosY);

        // atualiza o raio da câmera com base no deslocamento do mouse 
        radius += dx - dy;

        // restringe o raio (3 a 15 unidades)
        radius = radius < 3.0f ? 3.0f : (radius > 15.0f ? 15.0f : radius);
    }

    lastMousePosX = mousePosX;
    lastMousePosY = mousePosY;

    // converte coordenadas esféricas para cartesianas
    float x = radius * sinf(phi) * cosf(theta);
    float z = radius * sinf(phi) * sinf(theta);
    float y = radius * cosf(phi);

    // constrói a matriz da câmera (view matrix)
    XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&View, view);

    // carrega matriz de projeção em uma XMMATRIX
    XMMATRIX proj = XMLoadFloat4x4(&Proj);

    XMMATRIX projOrtho = XMMatrixOrthographicLH(10, 10, 1.0f, 100.0f);

    // ajusta o buffer constante de cada objeto
    // cena original
    for (auto& obj : scene)
    {
        // carrega matriz de mundo em uma XMMATRIX
        XMMATRIX world = XMLoadFloat4x4(&obj.world);

        // constrói matriz combinada (world x view x proj)
        XMMATRIX WorldViewProj = world * view * proj;

        // atualiza o buffer constante com a matriz combinada
        ObjectConstants constants;
        XMStoreFloat4x4(&constants.WorldViewProj, XMMatrixTranspose(WorldViewProj));
        obj.mesh->CopyConstants(&constants);
    }

    // constrói a matriz da câmera (view matrix)
    XMVECTOR pos1 = XMVectorSet(0, 0, 2.0f, 1.0f);
    XMVECTOR target1 = XMVectorZero();
    XMVECTOR up1 = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX view1 = XMMatrixLookAtLH(pos1, target1, up1);
    for (auto& obj : sceneTopEsq)
    {
        // carrega matriz de mundo em uma XMMATRIX
        XMMATRIX world = XMLoadFloat4x4(&obj.world);

        // constrói matriz combinada (world x view x proj)
        XMMATRIX WorldViewProj = world * view1 * projOrtho;

        // atualiza o buffer constante com a matriz combinada
        ObjectConstants constants;
        XMStoreFloat4x4(&constants.WorldViewProj, XMMatrixTranspose(WorldViewProj));
        obj.mesh->CopyConstants(&constants);
    }

    XMVECTOR pos2 = XMVectorSet(5.0f, 0, 0.0f, 1.0f);
    XMVECTOR target2 = XMVectorZero();
    XMVECTOR up2 = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX view2 = XMMatrixLookAtLH(pos2, target2, up2);
    for (auto& obj : sceneBaixEsq)
    {
        // carrega matriz de mundo em uma XMMATRIX
        XMMATRIX world = XMLoadFloat4x4(&obj.world);

        // constrói matriz combinada (world x view x proj)
        XMMATRIX WorldViewProj = world * view2 * projOrtho;

        // atualiza o buffer constante com a matriz combinada
        ObjectConstants constants;
        XMStoreFloat4x4(&constants.WorldViewProj, XMMatrixTranspose(WorldViewProj));
        obj.mesh->CopyConstants(&constants);
    }

    XMVECTOR pos3 = XMVectorSet(0.0f, 4.0f, 0.0f, 1.0f);
    XMVECTOR target3 = XMVectorZero();
    XMVECTOR up3 = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    XMMATRIX view3 = XMMatrixLookAtLH(pos3, target3, up3);
    for (auto& obj : sceneTopDir)
    {
        // carrega matriz de mundo em uma XMMATRIX
        XMMATRIX world = XMLoadFloat4x4(&obj.world);

        // constrói matriz combinada (world x view x proj)
        XMMATRIX WorldViewProj = world * view3 * projOrtho;

        // atualiza o buffer constante com a matriz combinada
        ObjectConstants constants;
        XMStoreFloat4x4(&constants.WorldViewProj, XMMatrixTranspose(WorldViewProj));
        obj.mesh->CopyConstants(&constants);
    }

    // linhas
    for (auto& obj : lines)
    {
        // lines vertical ok
        XMVECTOR pos5 = XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f);
        XMMATRIX view5 = XMMatrixLookAtLH(pos5, target3, up3);
        // carrega matriz de mundo em uma XMMATRIX
        XMMATRIX world = XMLoadFloat4x4(&obj.world);
        XMMATRIX x = XMMatrixOrthographicLH(10, 10, 1.0f, 100.0f);

        // constrói matriz combinada (world x view x proj)
        XMMATRIX WorldViewProj = world * view5 * x;

        // atualiza o buffer constante com a matriz combinada
        ObjectConstants constants;
        XMStoreFloat4x4(&constants.WorldViewProj, XMMatrixTranspose(WorldViewProj));
        lineH.mesh->CopyConstants(&constants);
        lineV.mesh->CopyConstants(&constants);
    }

    if (quadViewMode) {

        graphics->ResetCommands();

        // horizontal
        Box box2(0.01f, 1000.0f, 0.0f);

        for (auto& v : box2.vertices) v.color = XMFLOAT4(DirectX::Colors::Blue);

        XMStoreFloat4x4(&lineV.world,
            XMMatrixScaling(0.01f, 1.0f, 0.4f) *
            XMMatrixTranslation(0.0, 0.0f, -0.1f));

        lineV.mesh = new Mesh();
        lineV.mesh->VertexBuffer(box2.VertexData(), box2.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        lineV.mesh->IndexBuffer(box2.IndexData(), box2.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        lineV.mesh->ConstantBuffer(sizeof(ObjectConstants));
        lineV.submesh.indexCount = box2.IndexCount();
        lines.push_back(lineV);

        Box box1(150.0f, 0.11f, 0.0f);
        for (auto& v : box1.vertices) v.color = XMFLOAT4(DirectX::Colors::HotPink);

        XMStoreFloat4x4(&lineH.world,
            XMMatrixScaling(1.0f, 0.01f, 0.4f) *
            XMMatrixTranslation(0.0, 0.0f, 0.0f));

        lineH.mesh = new Mesh();
        lineH.mesh->VertexBuffer(box1.VertexData(), box1.VertexCount() * sizeof(Vertex), sizeof(Vertex));
        lineH.mesh->IndexBuffer(box1.IndexData(), box1.IndexCount() * sizeof(uint), DXGI_FORMAT_R32_UINT);
        lineH.mesh->ConstantBuffer(sizeof(ObjectConstants));
        lineH.submesh.indexCount = box1.IndexCount();
        lines.push_back(lineH);


        BuildRootSignature();
        BuildPipelineState();

        graphics->SubmitCommands();
    }
}

// ------------------------------------------------------------------------------

void Multi::Draw()
{
    graphics->Clear(pipelineState);

    if (quadViewMode) {

        for (auto& obj : lines)
        {
            // Comandos de configuração do pipeline
            ID3D12DescriptorHeap* descriptorHeaps[] = { obj.mesh->ConstantBufferHeap() };
            graphics->CommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
            graphics->CommandList()->SetGraphicsRootSignature(rootSignature);
            graphics->CommandList()->IASetVertexBuffers(0, 1, obj.mesh->VertexBufferView());
            graphics->CommandList()->IASetIndexBuffer(obj.mesh->IndexBufferView());
            graphics->CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

            // ajusta o buffer constante
            graphics->CommandList()->SetGraphicsRootDescriptorTable(0, obj.mesh->ConstantBufferHandle(0));

            graphics->CommandList()->DrawIndexedInstanced(
                obj.submesh.indexCount, 1,
                obj.submesh.startIndex,
                obj.submesh.baseVertex,
                0);
        }

        for (auto& lineV : lines)
        {
            // Comandos de configuração do pipeline
            ID3D12DescriptorHeap* descriptorHeaps[] = { lineV.mesh->ConstantBufferHeap() };
            graphics->CommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
            graphics->CommandList()->SetGraphicsRootSignature(rootSignature);
            graphics->CommandList()->IASetVertexBuffers(0, 1, lineV.mesh->VertexBufferView());
            graphics->CommandList()->IASetIndexBuffer(lineV.mesh->IndexBufferView());
            graphics->CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

            // Ajusta o buffer constante
            graphics->CommandList()->SetGraphicsRootDescriptorTable(0, lineV.mesh->ConstantBufferHandle(0));

            graphics->CommandList()->RSSetViewports(1, &lineVview); // Use a viewport adequada para lineV
            // Renderiza a linha vertical
            graphics->CommandList()->DrawIndexedInstanced(
                lineV.submesh.indexCount, 1,
                lineV.submesh.startIndex,
                lineV.submesh.baseVertex,
                0);
        }

        for (auto& lineH : lines)
        {
            // Comandos de configuração do pipeline
            ID3D12DescriptorHeap* descriptorHeaps[] = { lineH.mesh->ConstantBufferHeap() };
            graphics->CommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
            graphics->CommandList()->SetGraphicsRootSignature(rootSignature);
            graphics->CommandList()->IASetVertexBuffers(0, 1, lineH.mesh->VertexBufferView());
            graphics->CommandList()->IASetIndexBuffer(lineH.mesh->IndexBufferView());
            graphics->CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

            // ajusta o buffer constante
            graphics->CommandList()->SetGraphicsRootDescriptorTable(0, lineH.mesh->ConstantBufferHandle(0));

            graphics->CommandList()->RSSetViewports(1, &lineHview);
            // Primeira viewport (Front)
            graphics->CommandList()->DrawIndexedInstanced(
                lineH.submesh.indexCount, 1,
                lineH.submesh.startIndex,
                lineH.submesh.baseVertex,
                0);
        }

        // cima esquerda
        for (auto& obj : sceneTopEsq)
        {
            // Comandos de configuração do pipeline
            ID3D12DescriptorHeap* descriptorHeaps[] = { obj.mesh->ConstantBufferHeap() };
            graphics->CommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
            graphics->CommandList()->SetGraphicsRootSignature(rootSignature);
            graphics->CommandList()->IASetVertexBuffers(0, 1, obj.mesh->VertexBufferView());
            graphics->CommandList()->IASetIndexBuffer(obj.mesh->IndexBufferView());
            graphics->CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            // ajusta o buffer constante
            graphics->CommandList()->SetGraphicsRootDescriptorTable(0, obj.mesh->ConstantBufferHandle(0));

            graphics->CommandList()->RSSetViewports(1, &viewFront);
            // Primeira viewport (Front)
            graphics->CommandList()->DrawIndexedInstanced(
                obj.submesh.indexCount, 1,
                obj.submesh.startIndex,
                obj.submesh.baseVertex,
                0);
        }

        // baixo esquerda
        for (auto& obj : sceneBaixEsq)
        {
            // Comandos de configuração do pipeline
            ID3D12DescriptorHeap* descriptorHeaps[] = { obj.mesh->ConstantBufferHeap() };
            graphics->CommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
            graphics->CommandList()->SetGraphicsRootSignature(rootSignature);
            graphics->CommandList()->IASetVertexBuffers(0, 1, obj.mesh->VertexBufferView());
            graphics->CommandList()->IASetIndexBuffer(obj.mesh->IndexBufferView());
            graphics->CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            graphics->CommandList()->SetGraphicsRootDescriptorTable(0, obj.mesh->ConstantBufferHandle(0));

            graphics->CommandList()->RSSetViewports(1, &viewTop); // Altere para viewTop
            graphics->CommandList()->DrawIndexedInstanced(
                obj.submesh.indexCount, 1,
                obj.submesh.startIndex,
                obj.submesh.baseVertex,
                0);
        }

        // topo dir
        for (auto& obj : sceneTopDir)
        {
            // Comandos de configuração do pipeline
            ID3D12DescriptorHeap* descriptorHeaps[] = { obj.mesh->ConstantBufferHeap() };
            graphics->CommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
            graphics->CommandList()->SetGraphicsRootSignature(rootSignature);
            graphics->CommandList()->IASetVertexBuffers(0, 1, obj.mesh->VertexBufferView());
            graphics->CommandList()->IASetIndexBuffer(obj.mesh->IndexBufferView());
            graphics->CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            graphics->CommandList()->SetGraphicsRootDescriptorTable(0, obj.mesh->ConstantBufferHandle(0));

            graphics->CommandList()->RSSetViewports(1, &viewRight); // Altere para viewRight
            graphics->CommandList()->DrawIndexedInstanced(
                obj.submesh.indexCount, 1,
                obj.submesh.startIndex,
                obj.submesh.baseVertex,
                0);
        }

        for (auto& obj : scene)
        {
            // Comandos de configuração do pipeline
            ID3D12DescriptorHeap* descriptorHeaps[] = { obj.mesh->ConstantBufferHeap() };
            graphics->CommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
            graphics->CommandList()->SetGraphicsRootSignature(rootSignature);
            graphics->CommandList()->IASetVertexBuffers(0, 1, obj.mesh->VertexBufferView());
            graphics->CommandList()->IASetIndexBuffer(obj.mesh->IndexBufferView());
            graphics->CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            graphics->CommandList()->SetGraphicsRootDescriptorTable(0, obj.mesh->ConstantBufferHandle(0));


            // Quarta viewport (Perspective)
            graphics->CommandList()->RSSetViewports(1, &viewPerspective); // Altere para viewPerspective
            graphics->CommandList()->DrawIndexedInstanced(
                obj.submesh.indexCount, 1,
                obj.submesh.startIndex,
                obj.submesh.baseVertex,
                0);
        }
        // apresenta o backbuffer na tela
        graphics->Present();
    }
    else {
        // desenha objetos da cena
        graphics->Clear(pipelineState);
        for (auto& obj : scene)
        {
            // comandos de configuração do pipeline
            ID3D12DescriptorHeap* descriptorHeaps[] = { obj.mesh->ConstantBufferHeap() };
            graphics->CommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
            graphics->CommandList()->SetGraphicsRootSignature(rootSignature);
            graphics->CommandList()->IASetVertexBuffers(0, 1, obj.mesh->VertexBufferView());
            graphics->CommandList()->IASetIndexBuffer(obj.mesh->IndexBufferView());
            graphics->CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            // ajusta o buffer constante associado ao vertex shader
            graphics->CommandList()->SetGraphicsRootDescriptorTable(0, obj.mesh->ConstantBufferHandle(0));

            // desenha objeto
            graphics->CommandList()->DrawIndexedInstanced(
                obj.submesh.indexCount, 1,
                obj.submesh.startIndex,
                obj.submesh.baseVertex,
                0);

        }

        // trocar pra line list a linha
        // apresenta o backbuffer na tela
        graphics->Present();
    }

    //graphics->Present();

}

void Multi::Finalize()
{
    rootSignature->Release();
    pipelineState->Release();

    for (auto& obj : scene)
        delete obj.mesh;

    for (auto& obj : sceneBaixEsq)
        delete obj.mesh;

    for (auto& obj : sceneTopDir)
        delete obj.mesh;

    for (auto& obj : sceneTopEsq)
        delete obj.mesh;
}


// ------------------------------------------------------------------------------
//                                     D3D                                      
// ------------------------------------------------------------------------------

void Multi::BuildRootSignature()
{
    // cria uma única tabela de descritores de CBVs
    D3D12_DESCRIPTOR_RANGE cbvTable = {};
    cbvTable.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    cbvTable.NumDescriptors = 1;
    cbvTable.BaseShaderRegister = 0;
    cbvTable.RegisterSpace = 0;
    cbvTable.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // define parâmetro raiz com uma tabela
    D3D12_ROOT_PARAMETER rootParameters[1];
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &cbvTable;

    // uma assinatura raiz é um vetor de parâmetros raiz
    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = 1;
    rootSigDesc.pParameters = rootParameters;
    rootSigDesc.NumStaticSamplers = 0;
    rootSigDesc.pStaticSamplers = nullptr;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // serializa assinatura raiz
    ID3DBlob* serializedRootSig = nullptr;
    ID3DBlob* error = nullptr;

    ThrowIfFailed(D3D12SerializeRootSignature(
        &rootSigDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &serializedRootSig,
        &error));

    if (error != nullptr)
    {
        OutputDebugString((char*)error->GetBufferPointer());
    }

    // cria uma assinatura raiz com um único slot que aponta para  
    // uma faixa de descritores consistindo de um único buffer constante
    ThrowIfFailed(graphics->Device()->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature)));
}

// ------------------------------------------------------------------------------

void Multi::BuildPipelineState()
{
    // --------------------
    // --- Input Layout ---
    // --------------------

    D3D12_INPUT_ELEMENT_DESC inputLayout[2] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // --------------------
    // ----- Shaders ------
    // --------------------

    ID3DBlob* vertexShader;
    ID3DBlob* pixelShader;

    D3DReadFileToBlob(L"Shaders/Vertex.cso", &vertexShader);
    D3DReadFileToBlob(L"Shaders/Pixel.cso", &pixelShader);

    // --------------------
    // ---- Rasterizer ----
    // --------------------

    D3D12_RASTERIZER_DESC rasterizer = {};
    //rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer.FillMode = D3D12_FILL_MODE_WIREFRAME;
    rasterizer.CullMode = D3D12_CULL_MODE_BACK;
    rasterizer.FrontCounterClockwise = FALSE;
    rasterizer.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizer.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizer.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizer.DepthClipEnable = TRUE;
    rasterizer.MultisampleEnable = FALSE;
    rasterizer.AntialiasedLineEnable = FALSE;
    rasterizer.ForcedSampleCount = 0;
    rasterizer.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // ---------------------
    // --- Color Blender ---
    // ---------------------

    D3D12_BLEND_DESC blender = {};
    blender.AlphaToCoverageEnable = FALSE;
    blender.IndependentBlendEnable = FALSE;
    const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
    {
        FALSE,FALSE,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL,
    };
    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        blender.RenderTarget[i] = defaultRenderTargetBlendDesc;

    // ---------------------
    // --- Depth Stencil ---
    // ---------------------

    D3D12_DEPTH_STENCIL_DESC depthStencil = {};
    depthStencil.DepthEnable = TRUE;
    depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencil.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    depthStencil.StencilEnable = FALSE;
    depthStencil.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    depthStencil.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp =
    { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
    depthStencil.FrontFace = defaultStencilOp;
    depthStencil.BackFace = defaultStencilOp;

    // -----------------------------------
    // --- Pipeline State Object (PSO) ---
    // -----------------------------------

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.pRootSignature = rootSignature;
    pso.VS = { reinterpret_cast<BYTE*>(vertexShader->GetBufferPointer()), vertexShader->GetBufferSize() };
    pso.PS = { reinterpret_cast<BYTE*>(pixelShader->GetBufferPointer()), pixelShader->GetBufferSize() };
    pso.BlendState = blender;
    pso.SampleMask = UINT_MAX;
    pso.RasterizerState = rasterizer;
    pso.DepthStencilState = depthStencil;
    pso.InputLayout = { inputLayout, 2 };
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    pso.SampleDesc.Count = graphics->Antialiasing();
    pso.SampleDesc.Quality = graphics->Quality();
    graphics->Device()->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pipelineState));

    vertexShader->Release();
    pixelShader->Release();
}



void Multi::BuildPipelineStateFront()
{
    // --------------------
    // --- Input Layout ---
    // --------------------

    D3D12_INPUT_ELEMENT_DESC inputLayout[2] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // --------------------
    // ----- Shaders ------
    // --------------------

    ID3DBlob* vertexShader;
    ID3DBlob* pixelShader;

    D3DReadFileToBlob(L"Shaders/Vertex.cso", &vertexShader);
    D3DReadFileToBlob(L"Shaders/Pixel.cso", &pixelShader);

    // --------------------
    // ---- Rasterizer ----
    // --------------------

    D3D12_RASTERIZER_DESC rasterizer = {};
    //rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer.FillMode = D3D12_FILL_MODE_WIREFRAME;
    rasterizer.CullMode = D3D12_CULL_MODE_FRONT;
    rasterizer.FrontCounterClockwise = FALSE;
    rasterizer.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizer.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizer.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizer.DepthClipEnable = TRUE;
    rasterizer.MultisampleEnable = FALSE;
    rasterizer.AntialiasedLineEnable = FALSE;
    rasterizer.ForcedSampleCount = 0;
    rasterizer.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // ---------------------
    // --- Color Blender ---
    // ---------------------

    D3D12_BLEND_DESC blender = {};
    blender.AlphaToCoverageEnable = FALSE;
    blender.IndependentBlendEnable = FALSE;
    const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
    {
        FALSE,FALSE,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL,
    };
    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        blender.RenderTarget[i] = defaultRenderTargetBlendDesc;

    // ---------------------
    // --- Depth Stencil ---
    // ---------------------

    D3D12_DEPTH_STENCIL_DESC depthStencil = {};
    depthStencil.DepthEnable = TRUE;
    depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencil.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    depthStencil.StencilEnable = FALSE;
    depthStencil.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    depthStencil.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp =
    { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
    depthStencil.FrontFace = defaultStencilOp;
    depthStencil.BackFace = defaultStencilOp;

    // -----------------------------------
    // --- Pipeline State Object (PSO) ---
    // -----------------------------------

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.pRootSignature = rootSignature;
    pso.VS = { reinterpret_cast<BYTE*>(vertexShader->GetBufferPointer()), vertexShader->GetBufferSize() };
    pso.PS = { reinterpret_cast<BYTE*>(pixelShader->GetBufferPointer()), pixelShader->GetBufferSize() };
    pso.BlendState = blender;
    pso.SampleMask = UINT_MAX;
    pso.RasterizerState = rasterizer;
    pso.DepthStencilState = depthStencil;
    pso.InputLayout = { inputLayout, 2 };
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    pso.SampleDesc.Count = graphics->Antialiasing();
    pso.SampleDesc.Quality = graphics->Quality();
    graphics->Device()->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pipelineState));

    vertexShader->Release();
    pixelShader->Release();
}

void Multi::BuildPipelineStateNone()
{
    // --------------------
    // --- Input Layout ---
    // --------------------

    D3D12_INPUT_ELEMENT_DESC inputLayout[2] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // --------------------
    // ----- Shaders ------
    // --------------------

    ID3DBlob* vertexShader;
    ID3DBlob* pixelShader;

    D3DReadFileToBlob(L"Shaders/Vertex.cso", &vertexShader);
    D3DReadFileToBlob(L"Shaders/Pixel.cso", &pixelShader);

    // --------------------
    // ---- Rasterizer ----
    // --------------------

    D3D12_RASTERIZER_DESC rasterizer = {};
    //rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer.FillMode = D3D12_FILL_MODE_WIREFRAME;
    rasterizer.CullMode = D3D12_CULL_MODE_NONE;
    rasterizer.FrontCounterClockwise = FALSE;
    rasterizer.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizer.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizer.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizer.DepthClipEnable = TRUE;
    rasterizer.MultisampleEnable = FALSE;
    rasterizer.AntialiasedLineEnable = FALSE;
    rasterizer.ForcedSampleCount = 0;
    rasterizer.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // ---------------------
    // --- Color Blender ---
    // ---------------------

    D3D12_BLEND_DESC blender = {};
    blender.AlphaToCoverageEnable = FALSE;
    blender.IndependentBlendEnable = FALSE;
    const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
    {
        FALSE,FALSE,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL,
    };
    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        blender.RenderTarget[i] = defaultRenderTargetBlendDesc;

    // ---------------------
    // --- Depth Stencil ---
    // ---------------------

    D3D12_DEPTH_STENCIL_DESC depthStencil = {};
    depthStencil.DepthEnable = TRUE;
    depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencil.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    depthStencil.StencilEnable = FALSE;
    depthStencil.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    depthStencil.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp =
    { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
    depthStencil.FrontFace = defaultStencilOp;
    depthStencil.BackFace = defaultStencilOp;

    // -----------------------------------
    // --- Pipeline State Object (PSO) ---
    // -----------------------------------

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.pRootSignature = rootSignature;
    pso.VS = { reinterpret_cast<BYTE*>(vertexShader->GetBufferPointer()), vertexShader->GetBufferSize() };
    pso.PS = { reinterpret_cast<BYTE*>(pixelShader->GetBufferPointer()), pixelShader->GetBufferSize() };
    pso.BlendState = blender;
    pso.SampleMask = UINT_MAX;
    pso.RasterizerState = rasterizer;
    pso.DepthStencilState = depthStencil;
    pso.InputLayout = { inputLayout, 2 };
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    pso.SampleDesc.Count = graphics->Antialiasing();
    pso.SampleDesc.Quality = graphics->Quality();
    graphics->Device()->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pipelineState));

    vertexShader->Release();
    pixelShader->Release();
}

// ------------------------------------------------------------------------------
//                                  WinMain                                      
// ------------------------------------------------------------------------------

int APIENTRY WinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow)
{
    try
    {
        // cria motor e configura a janela
        Engine* engine = new Engine();
        engine->window->Mode(WINDOWED);
        engine->window->Size(1024, 720);
        engine->window->Color(25, 25, 25);
        engine->window->Title("Multi");
        engine->window->Icon(IDI_ICON);
        engine->window->Cursor(IDC_CURSOR);
        engine->window->LostFocus(Engine::Pause);
        engine->window->InFocus(Engine::Resume);

        // cria e executa a aplicação
        engine->Start(new Multi());

        // finaliza execução
        delete engine;
    }
    catch (Error& e)
    {
        // exibe mensagem em caso de erro
        MessageBox(nullptr, e.ToString().data(), "Multi", MB_OK);
    }

    return 0;
}

// ----------------------------------------------------------------------------