#include <SFML/Graphics.hpp>
#include <vector>
#include <list>
#include <queue>
#include <random>
#include <string>
#include "Cell.h"

const int GRID_WIDTH  = 20;
const int GRID_HEIGHT = 20;
const int CELL_SIZE   = 30; // pixels per cell
const sf::Time MOVE_DELAY = sf::milliseconds(150); // time between snake steps
const int OBSTACLE_COUNT = 10;

const sf::Time BOOSTED_MOVE_DELAY = sf::milliseconds(75);
const sf::Time FIRST_POWERUP      = sf::seconds(8);
const sf::Time POWERUP_INTERVAL   = sf::seconds(15);
const sf::Time POWERUP_LIFETIME   = sf::seconds(6);
const sf::Time BOOST_DURATION     = sf::seconds(5);
const sf::Time FRENZY_DURATION    = sf::seconds(5);

std::mt19937 rng(std::random_device{}());

struct Position
{
    int x;
    int y;
};

enum class EventType
{
    SpawnPowerUp,
    PowerUpExpires,
    SpeedBoostEnds,
    FrenzyEnds
};

struct TimedEvent
{
    sf::Time dueTime;
    EventType type;
    int cellIndex;
};

struct LaterFirst
{
    bool operator()(const TimedEvent& a, const TimedEvent& b) const
    {
        return a.dueTime > b.dueTime;
    }
};

int toIndex(int x, int y)
{
    return y * GRID_WIDTH + x;
}

int toIndex(Position p)
{
    return toIndex(p.x, p.y);
}

int spawnInEmptyCell(std::vector<Cell>& grid, CellState state)
{
    std::vector<int> emptyCells;
    for (int i = 0; i < static_cast<int>(grid.size()); i++)
        if (grid[i].getState() == CellState::Empty)
            emptyCells.push_back(i);

    if (emptyCells.empty())
        return -1;

    std::uniform_int_distribution<int> pick(0, static_cast<int>(emptyCells.size()) - 1);
    int index = emptyCells[pick(rng)];
    grid[index].setState(state);
    return index;
}

void placeObstacles(std::vector<Cell>& grid, int safeRow)
{
    std::uniform_int_distribution<int> pick(0, static_cast<int>(grid.size()) - 1);

    int placed = 0;
    while (placed < OBSTACLE_COUNT)
    {
        Cell& cell = grid[pick(rng)];
        if (cell.getState() != CellState::Empty || cell.y == safeRow)
            continue;
        cell.setState(CellState::Obstacle);
        placed++;
    }
}

bool moveSnake(std::list<Position>& snake, std::vector<Cell>& grid, Position direction, int& score, bool frenzyActive, CellState& eaten)
{
    Position head = snake.front();
    Position newHead{ head.x + direction.x, head.y + direction.y };

    // Wall
    if (newHead.x < 0 || newHead.x >= GRID_WIDTH || newHead.y < 0 || newHead.y >= GRID_HEIGHT)
        return false;

    eaten = grid[toIndex(newHead)].getState();
    bool ateFood = eaten == CellState::Food;
    bool grow = ateFood && !frenzyActive;

    if (!grow)
    {
        Position tail = snake.back();
        snake.pop_back();
        grid[toIndex(tail)].setState(CellState::Empty);
    }

    CellState target = grid[toIndex(newHead)].getState();
    if (target == CellState::Snake || target == CellState::Obstacle)
        return false;

    snake.push_front(newHead);
    grid[toIndex(newHead)].setState(CellState::Snake);

    if (ateFood)
    {
        score++;
        if (!frenzyActive && spawnInEmptyCell(grid, CellState::Food) == -1)
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

void drawGameOver(sf::RenderWindow& window, const sf::Font& font)
{
    sf::Vector2f windowSize(static_cast<float>(GRID_WIDTH * CELL_SIZE), static_cast<float>(GRID_HEIGHT * CELL_SIZE));

    sf::RectangleShape overlay(windowSize);
    overlay.setFillColor(sf::Color(0, 0, 0, 150));
    window.draw(overlay);

    sf::Text text(font, "Game Over!", 56);
    text.setFillColor(sf::Color::Red);
    sf::FloatRect bounds = text.getLocalBounds();
    text.setOrigin(bounds.position + bounds.size / 2.f);
    text.setPosition(windowSize / 2.f);
    window.draw(text);

    sf::Text hint(font, "Press R to restart", 24);
    hint.setFillColor(sf::Color::White);
    sf::FloatRect hintBounds = hint.getLocalBounds();
    hint.setOrigin(hintBounds.position + hintBounds.size / 2.f);
    hint.setPosition({ windowSize.x / 2.f, windowSize.y / 2.f + 50.f });
    window.draw(hint);
}

void displayGrid(sf::RenderWindow& window, const std::vector<Cell>& grid, const std::list<Position>& snake, bool gameOver, const sf::Font& font, int score)
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
        case CellState::Obstacle: cellShape.setFillColor(sf::Color(128, 128, 128)); break;
        case CellState::SpeedBoost: cellShape.setFillColor(sf::Color::Cyan); break;
        case CellState::Frenzy: cellShape.setFillColor(sf::Color::Magenta); break;
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

    sf::Text scoreText(font, "Score: " + std::to_string(score), 22);
    scoreText.setFillColor(sf::Color::White);
    scoreText.setPosition({ 8.f, 4.f });
    window.draw(scoreText);

    if (gameOver)
        drawGameOver(window, font);

    window.display();
}

void resetGame(std::vector<Cell>& grid, std::list<Position>& snake)
{
    grid.clear();
    for (int y = 0; y < GRID_HEIGHT; y++)
        for (int x = 0; x < GRID_WIDTH; x++)
            grid.push_back(Cell(x, y));

    snake.clear();
    int startX = GRID_WIDTH / 2;
    int startY = GRID_HEIGHT / 2;
    for (int i = 0; i < 3; i++)
    {
        Position p{ startX - i, startY };
        snake.push_back(p);
        grid[toIndex(p)].setState(CellState::Snake);
    }

    placeObstacles(grid, startY);
    spawnInEmptyCell(grid, CellState::Food);
}

int main()
{
    std::vector<Cell> grid;
    grid.reserve(GRID_WIDTH * GRID_HEIGHT);
    std::list<Position> snake;
    resetGame(grid, snake);

    sf::RenderWindow window(sf::VideoMode({ GRID_WIDTH * CELL_SIZE, GRID_HEIGHT * CELL_SIZE }), "Snake");
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.openFromFile("C:/Windows/Fonts/arial.ttf"))
        return 1;

    Position direction{ 1, 0 };
    Position nextDirection = direction;

    sf::Clock moveClock;
    bool gameOver = false;
    int score = 0;

    std::priority_queue<TimedEvent, std::vector<TimedEvent>, LaterFirst> events;
    sf::Clock gameClock;
    sf::Time moveDelay = MOVE_DELAY;
    sf::Time boostEndsAt;
    sf::Time frenzyEndsAt;
    bool frenzyActive = false;

    events.push({ FIRST_POWERUP, EventType::SpawnPowerUp, -1 });

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
                if (gameOver && key->code == sf::Keyboard::Key::R)
                {
                    resetGame(grid, snake);
                    direction = { 1, 0 };
                    nextDirection = direction;
                    score = 0;
                    gameOver = false;
                    moveClock.restart();

                    events = {};
                    gameClock.restart();
                    moveDelay = MOVE_DELAY;
                    frenzyActive = false;
                    events.push({ FIRST_POWERUP, EventType::SpawnPowerUp, -1 });
                }
                else if (key->code == sf::Keyboard::Key::Up && direction.y != 1)
                    nextDirection = { 0, -1 };
                else if (key->code == sf::Keyboard::Key::Down && direction.y != -1)
                    nextDirection = { 0, 1 };
                else if (key->code == sf::Keyboard::Key::Left && direction.x != 1)
                    nextDirection = { -1, 0 };
                else if (key->code == sf::Keyboard::Key::Right && direction.x != -1)
                    nextDirection = { 1, 0 };
            }
        }

        sf::Time now = gameClock.getElapsedTime();

        while (!gameOver && !events.empty() && events.top().dueTime <= now)
        {
            TimedEvent e = events.top();
            events.pop();

            switch (e.type)
            {
            case EventType::SpawnPowerUp:
            {
                CellState type = (rng() % 2 == 0) ? CellState::SpeedBoost : CellState::Frenzy;
                int index = spawnInEmptyCell(grid, type);
                if (index != -1)
                    events.push({ now + POWERUP_LIFETIME, EventType::PowerUpExpires, index });
                events.push({ now + POWERUP_INTERVAL, EventType::SpawnPowerUp, -1 });
                break;
            }
            case EventType::PowerUpExpires:
            {
                CellState s = grid[e.cellIndex].getState();
                if (s == CellState::SpeedBoost || s == CellState::Frenzy)
                    grid[e.cellIndex].setState(CellState::Empty);
                break;
            }
            case EventType::SpeedBoostEnds:
                if (now >= boostEndsAt)
                    moveDelay = MOVE_DELAY;
                break;
            case EventType::FrenzyEnds:
                if (now >= frenzyEndsAt)
                {
                    for (Cell& cell : grid)
                        if (cell.getState() == CellState::Food)
                            cell.setState(CellState::Empty);
                    spawnInEmptyCell(grid, CellState::Food);
                    frenzyActive = false;
                }
                break;
            }
        }

        if (!gameOver && moveClock.getElapsedTime() >= moveDelay)
        {
            moveClock.restart();
            direction = nextDirection;

            CellState eaten = CellState::Empty;
            if (!moveSnake(snake, grid, direction, score, frenzyActive, eaten))
            {
                gameOver = true;
            }
            else if (eaten == CellState::SpeedBoost)
            {
                moveDelay = BOOSTED_MOVE_DELAY;
                boostEndsAt = now + BOOST_DURATION;
                events.push({ boostEndsAt, EventType::SpeedBoostEnds, -1 });
            }
            else if (eaten == CellState::Frenzy)
            {
                frenzyActive = true;
                for (Cell& cell : grid)
                    if (cell.getState() == CellState::Empty)
                        cell.setState(CellState::Food);
                frenzyEndsAt = now + FRENZY_DURATION;
                events.push({ frenzyEndsAt, EventType::FrenzyEnds, -1 });
            }
        }

        displayGrid(window, grid, snake, gameOver, font, score);
    }

    return 0;
}
