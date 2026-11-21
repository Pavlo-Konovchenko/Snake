#include <SFML/Graphics.hpp>
#include <vector>
#include <list>
#include <random>
#include <string>
#include "Cell.h"

const int GRID_WIDTH  = 20;
const int GRID_HEIGHT = 20;
const int CELL_SIZE   = 30; // pixels per cell
const sf::Time MOVE_DELAY = sf::milliseconds(150); // time between snake steps

struct Position
{
    int x;
    int y;
};

int toIndex(int x, int y)
{
    return y * GRID_WIDTH + x;
}

int toIndex(Position p)
{
    return toIndex(p.x, p.y);
}

bool spawnFood(std::vector<Cell>& grid)
{
    static std::mt19937 rng(std::random_device{}());

    std::vector<int> emptyCells;
    for (int i = 0; i < static_cast<int>(grid.size()); i++)
        if (grid[i].getState() == CellState::Empty)
            emptyCells.push_back(i);

    if (emptyCells.empty())
        return false;

    std::uniform_int_distribution<int> pick(0, static_cast<int>(emptyCells.size()) - 1);
    grid[emptyCells[pick(rng)]].setState(CellState::Food);
    return true;
}

bool moveSnake(std::list<Position>& snake, std::vector<Cell>& grid, Position direction, int& score)
{
    Position head = snake.front();
    Position newHead{ head.x + direction.x, head.y + direction.y };

    // Wall
    if (newHead.x < 0 || newHead.x >= GRID_WIDTH || newHead.y < 0 || newHead.y >= GRID_HEIGHT)
        return false;

    bool ateFood = grid[toIndex(newHead)].getState() == CellState::Food;

    if (!ateFood)
    {
        Position tail = snake.back();
        snake.pop_back();
        grid[toIndex(tail)].setState(CellState::Empty);
    }

    if (grid[toIndex(newHead)].getState() == CellState::Snake)
        return false;

    snake.push_front(newHead);
    grid[toIndex(newHead)].setState(CellState::Snake);

    if (ateFood)
    {
        score++;
        if (!spawnFood(grid))
            return false; // board is full
    }

    return true;
}

sf::Angle directionAngle(Position d)
{
    if (d.x == 1)  return sf::degrees(0);
    if (d.y == 1)  return sf::degrees(90);
    if (d.x == -1) return sf::degrees(180);
    return sf::degrees(270);
}

void drawSnakeEnd(sf::RenderWindow& window, Position cellPos, Position direction, bool withEyes)
{
    float half = (CELL_SIZE - 1) / 2.f;

    sf::Transform transform;
    transform.translate({ cellPos.x * CELL_SIZE + half, cellPos.y * CELL_SIZE + half });
    transform.rotate(directionAngle(direction));

    sf::ConvexShape triangle(3);
    triangle.setPoint(0, { -half, -half });
    triangle.setPoint(1, { -half,  half });
    triangle.setPoint(2, {  half,  0.f });
    triangle.setFillColor(sf::Color::Green);
    window.draw(triangle, transform);

    if (!withEyes)
        return;

    float eyeRadius = half * 0.23f;
    float pupilRadius = half * 0.11f;

    sf::CircleShape eye(eyeRadius);
    eye.setOrigin({ eyeRadius, eyeRadius });
    eye.setFillColor(sf::Color::White);

    sf::CircleShape pupil(pupilRadius);
    pupil.setOrigin({ pupilRadius, pupilRadius });
    pupil.setFillColor(sf::Color::Black);

    for (float side : { -1.f, 1.f })
    {
        eye.setPosition({ -half * 0.4f, side * half * 0.43f });
        pupil.setPosition({ -half * 0.3f, side * half * 0.43f });
        window.draw(eye, transform);
        window.draw(pupil, transform);
    }
}

void displayGrid(sf::RenderWindow& window, const std::vector<Cell>& grid, const std::list<Position>& snake)
{
    sf::RectangleShape cellShape({ CELL_SIZE - 1.f, CELL_SIZE - 1.f });
    sf::Color emptyColor(40, 40, 40);

    Position head = snake.front();
    Position neck = *std::next(snake.begin());
    Position tail = snake.back();
    Position beforeTail = *std::prev(snake.end(), 2);

    int headIndex = toIndex(head);
    int tailIndex = toIndex(tail);

    window.clear(sf::Color::Black);

    for (const Cell& cell : grid)
    {
        int index = toIndex(cell.x, cell.y);

        switch (cell.getState())
        {
        case CellState::Empty: cellShape.setFillColor(emptyColor);     break;
        case CellState::Food:  cellShape.setFillColor(sf::Color::Red); break;
        case CellState::Snake:
            if (index == headIndex || index == tailIndex)
                cellShape.setFillColor(emptyColor);
            else
                cellShape.setFillColor(sf::Color::Green);
            break;
        }
        cellShape.setPosition({ static_cast<float>(cell.x * CELL_SIZE), static_cast<float>(cell.y * CELL_SIZE) });
        window.draw(cellShape);
    }

    drawSnakeEnd(window, tail, { tail.x - beforeTail.x, tail.y - beforeTail.y }, false);
    drawSnakeEnd(window, head, { head.x - neck.x, head.y - neck.y }, true);

    window.display();
}

int main()
{
    std::vector<Cell> grid;
    grid.reserve(GRID_WIDTH * GRID_HEIGHT);
    for (int y = 0; y < GRID_HEIGHT; y++)
        for (int x = 0; x < GRID_WIDTH; x++)
            grid.push_back(Cell(x, y));

    std::list<Position> snake;
    int startX = GRID_WIDTH / 2;
    int startY = GRID_HEIGHT / 2;
    for (int i = 0; i < 3; i++)
    {
        Position p{ startX - i, startY };
        snake.push_back(p);
        grid[toIndex(p)].setState(CellState::Snake);
    }

    spawnFood(grid);

    sf::RenderWindow window(sf::VideoMode({ GRID_WIDTH * CELL_SIZE, GRID_HEIGHT * CELL_SIZE }), "Snake - Score: 0");
    window.setFramerateLimit(60);

    Position direction{ 1, 0 };
    Position nextDirection = direction;

    sf::Clock moveClock;
    bool gameOver = false;
    int score = 0;

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
            else if (const auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                if (key->code == sf::Keyboard::Key::Up && direction.y != 1)
                    nextDirection = { 0, -1 };
                else if (key->code == sf::Keyboard::Key::Down && direction.y != -1)
                    nextDirection = { 0, 1 };
                else if (key->code == sf::Keyboard::Key::Left && direction.x != 1)
                    nextDirection = { -1, 0 };
                else if (key->code == sf::Keyboard::Key::Right && direction.x != -1)
                    nextDirection = { 1, 0 };
            }
        }

        if (!gameOver && moveClock.getElapsedTime() >= MOVE_DELAY)
        {
            moveClock.restart();
            direction = nextDirection;

            int oldScore = score;
            if (!moveSnake(snake, grid, direction, score))
                gameOver = true;

            if (score != oldScore)
                window.setTitle("Snake - Score: " + std::to_string(score));
        }

        displayGrid(window, grid, snake);
    }

    return 0;
}
