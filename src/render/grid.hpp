#pragma once

#include <cstddef>

struct Point3d;

class grid
{
  public:
    static constexpr int classicalResolutionX = ((33 * 4) + 1);
    static constexpr int classicalResolutionY = ((22 * 4) + 1);

    static int resolution_x;
    static int resolution_y;

    grid();
    ~grid();

    void setResolution(int width, int height);

    void initializeVertices();
    void initializeElements();

    void run();
    void draw();

    int extentX() { return resolution_x; }
    int extentY() { return resolution_y; }

    bool hitTest(const Point3d& pos, float radius, Point3d* hitPos = nullptr, Point3d* speed = nullptr);

    float brightness = 1.0f;

    std::size_t lightStartHorizontal = 0;
    std::size_t lightStartVertical = 0;
};
