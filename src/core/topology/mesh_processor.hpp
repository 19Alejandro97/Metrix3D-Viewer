#pragma once

class Mesh;

class MeshProcessor {
public:
    static void SmoothMesh(Mesh& mesh, int iterations, float lambda);
    static int FillHoles(Mesh& mesh);
};
