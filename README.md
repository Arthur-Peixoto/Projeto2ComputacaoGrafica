# TRABALHO PRÁTICO 2
O formato de arquivo OBJ é um dos formatos de arquivos comumente utilizados para definir a
geometria de um objeto 3D. Inicialmente desenvolvido pela Wavefront Technologies para seu
pacote de visualização, o formato é aberto e suportado por muitos softwares de modelagem e
visualização 3D, incluindo o Blender.
Construa uma aplicação gráfica que permita ao usuário visualizar e editar uma cena 3D formada
a partir das geometrias Box, Cylinder, Sphere, GeoSphere, Grid, Quad e também geometrias
carregadas a partir de arquivos OBJ.

- B -> Box 
- Q -> Quad
- C -> Cylinder 
- Teclas 1 a 5 -> leem Arquivos
- S -> Sphere
- TAB -> Seleciona
- G -> GeoSphere 
- DEL -> Remove
- P -> Plane (Grid) 
- V -> Modo de Visualização

A tecla V deve modificar o modo de visualização, apresentando a cena em 4 vistas diferentes:
Front, Top, Right e Perspective. Com exceção da perspectiva, as visualizações devem usar uma
projeção ortográfica na direção correspondente.
Clicar e arrastar o botão esquerdo do mouse deve girar a câmera usando coordenadas esféricas.
Clicar e arrastar o botão direito deve aproximar e afastar a câmera da cena. Apenas a câmera da
visualização perspectiva deve ser afetada por estas interações. As visualizações ortográficas não
devem ser alteradas.
As teclas numéricas devem carregar modelos 3D a partir de arquivos. Faça com que as teclas de
1 a 5 carreguem os modelos de teste: Ball, Capsule, House, Monkey e Thorus. Opcionalmente, as
demais teclas numéricas podem ser usadas para carregar outros modelos de sua escolha.
