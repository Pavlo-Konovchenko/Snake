#pragma once

enum class CellState
{
    Empty,
    Food,
    Snake,
    Obstacle,
    SpeedBoost,
    Frenzy
};

class Cell
{
public:
    int x;
    int y;

    Cell(int x, int y) : x(x), y(y), state(CellState::Empty) {}

    CellState getState() const { return state; }
    void setState(CellState s) { state = s; }

private:
    CellState state;
};
