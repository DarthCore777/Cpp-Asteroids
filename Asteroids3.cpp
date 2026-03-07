#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>
#include <ctime>
#include <iostream>
#include <filesystem>
#include <fstream>

struct HighScore
{
    std::string name;

    int score = 0;
};

class Game;

constexpr float PI = 3.1415926535f;

constexpr float TURN_SPEED = 200.f;
constexpr float ACCELERATION = 500.f;
constexpr float MAX_SPEED = 600.f;
constexpr float DRAG = 0.9998f;

constexpr float SHOOT_DELAY = 0.09f;
constexpr float BULLET_SPEED = 1200.f;
constexpr float MAX_BULLETS = 4.f;

float distance(sf::Vector2f a, sf::Vector2f b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

bool polygonCircleCollision(
    const std::vector<sf::Vector2f>& poly,
    sf::Vector2f circleCenter,
    float radius)
{
    for (const auto& p : poly)
    {
        if (distance(p, circleCenter) < radius)
            return true;
    }

    return false;
}

// ==================== COLLISION DETECTION ===================

bool checkCollision(const sf::ConvexShape& a, const sf::ConvexShape& b)
{
    return a.getGlobalBounds().intersects(b.getGlobalBounds());
}

// ===================== BASE ENTITY =====================

class Entity
{
public:

    sf::Vector2f getPosition() const
    {
        return position;
    }

    sf::Vector2f position;
    sf::Vector2f velocity;
    float angle;
    bool isAlive = true;

    Entity(sf::Vector2f pos = { 0.f, 0.f }, float ang = 0.f)
        : position(pos), angle(ang) {
    }

    virtual void update(float dt) {}
    virtual void render(sf::RenderWindow& window) {}
    virtual void handleEvent(const sf::Event& event) {}

    virtual ~Entity() = default;
};

class Game;

// ===================== BULLET =====================

class Bullet : public Entity
{
public:
    bool fromUFO = false;

    Bullet(sf::Vector2f pos, sf::Vector2f dir, bool ufoShot = false)
        : Entity(pos, 0.f), direction(dir)
    {
        fromUFO = ufoShot;

        shape.setRadius(3.f);
        shape.setFillColor(sf::Color::White);
        shape.setOrigin(3.f, 3.f);
    }

    sf::CircleShape& getShape() { return shape; }

    void update(float dt) override
    {
        lifetime -= dt;

        if (lifetime <= 0.0f)
        {
            isAlive = false;
        }
        
        if (fromUFO)
        {
            position += direction * (BULLET_SPEED * 0.5f) * dt;
        }
        else
        {
            position += direction * BULLET_SPEED * dt;
        }


        if (position.x > 1200.f)
            position.x = 0.f;
        else if (position.x < 0.f)
            position.x = 1200.f;

        if (position.y > 900.f)
            position.y = 0.f;
        else if (position.y < 0.f)
            position.y = 900.f;
        shape.setPosition(position);
    }

    void render(sf::RenderWindow& window) override
    {
        window.draw(shape);
    }

private:
    sf::Vector2f direction;
    sf::CircleShape shape;
    float lifetime = 0.6f;
};

// ===================== PLAYER =====================

class Player : public Entity
{
public:
    Player(Game& g);

    void update(float dt) override;
    void render(sf::RenderWindow& window) override;
    void handleEvent(const sf::Event& event) override;

    void explode();

    sf::ConvexShape& getShape()
    {
        return ship;
    }

    bool isInvincible() const
    {
        return invincible;
    }

    sf::Vector2f getVelocity() const
    {
        return velocity;
    }

    std::vector<sf::Vector2f> getWorldPoints() const
    {
        std::vector<sf::Vector2f> points;

        for (size_t i = 0; i < ship.getPointCount(); i++)
        {
            points.push_back(
                ship.getTransform().transformPoint(ship.getPoint(i))
            );
        }

        return points;
    }

private:
    float hyperspaceCooldown = 0.f;

    float invincibleTimer = 0.f;
    bool invincible = false;

    Game& game;

    sf::ConvexShape ship;
    sf::ConvexShape thruster;

    sf::Vector2f velocity{ 0.f, 0.f };

    float shootTimer = 0.f;

    bool isThrusting = false;
    float flickerTimer = 0.f;
    bool flickerVisible = true;
};

// ===================== ASTEROID =====================

class Asteroid : public Entity
{
public:

    enum class AsteroidSize
    {
        Large,
        Medium,
        Small
    };

    float getRadius() const
    {
        return radius;
    }

    AsteroidSize getSize() const
    {
        return size;
    }

    sf::Vector2f getVelocity() const
    {
        return velocity;
    }

    void setVelocity(sf::Vector2f v)
    {
        velocity = v;
    }

    sf::ConvexShape& getShape()
    {
        return shape;
    }

    Asteroid(sf::Vector2f pos, AsteroidSize s, int wave)
        : Entity(pos, 0.f), size(s)
    {
        // ================= SIZE SETTINGS =================
        float scale;

        if (size == AsteroidSize::Large)
        {
            radius = 50.f;
            scale = 1.f;
        }
        else if (size == AsteroidSize::Medium)
        {
            radius = 31.f;
            scale = 0.6f;
        }
        else
        {
            radius = 19.f;
            scale = 0.35f;
        }

        // ================= SPEED SETTINGS =================
        float baseSpeed;

        if (size == AsteroidSize::Large)
            baseSpeed = 60.f;
        else if (size == AsteroidSize::Medium)
            baseSpeed = 100.f;
        else
            baseSpeed = 160.f;

        float speed = baseSpeed +
            static_cast<float>(std::rand()) / RAND_MAX * 40.f;

        float angleRad =
            static_cast<float>(std::rand()) / RAND_MAX * 2.f * PI;

        velocity.x = std::cos(angleRad) * speed;
        velocity.y = std::sin(angleRad) * speed;

        rotationSpeed =
            -50.f + static_cast<float>(std::rand()) / RAND_MAX * 100.f;

        // ================= SHAPE (JAGGED) =================
        shape.setPointCount(12);

        for (int i = 0; i < 12; i++)
        {
            float angle = i * 2.f * PI / 12.f;

            float offset =
                0.75f + (static_cast<float>(std::rand()) / RAND_MAX) * 0.5f;

            float pointRadius = radius * offset;

            float x = std::cos(angle) * pointRadius;
            float y = std::sin(angle) * pointRadius;

            shape.setPoint(i, { x, y });
        }

        shape.setFillColor(sf::Color::Transparent);
        shape.setOutlineColor(sf::Color::White);
        shape.setOutlineThickness(2.f);
        shape.setPosition(position);

        float speedMultiplier = 1.0f + (wave * 0.1f);
        velocity *= speedMultiplier;


        auto bounds = shape.getLocalBounds();
        radius = std::max(bounds.width, bounds.height) / 2.f;
    }

    void update(float dt) override
    {
        position += velocity * dt;
        angle += rotationSpeed * dt;

        // Screen wrapping
        if (position.x > 1200.f)
            position.x = 0.f;
        else if (position.x < 0.f)
            position.x = 1200.f;

        if (position.y > 900.f)
            position.y = 0.f;
        else if (position.y < 0.f)
            position.y = 900.f;

        shape.setPosition(position);
        shape.setRotation(angle);
    }

    void render(sf::RenderWindow& window) override
    {
        window.draw(shape);
    }

private:
    AsteroidSize size;
    float radius;
    sf::Vector2f velocity;
    float rotationSpeed;
    sf::ConvexShape shape;
};

// ==============UFO============================

class UFO : public Entity
{
public:
    enum class UFOType
    {
        Large,
        Small
    };

    UFO(Game& g, UFOType t) : game(g), type(t)
    {
        if (type == UFOType::Small)
            shootInterval = 1.0f;
        else
            shootInterval = 1.5f;

        float y = 50.f + static_cast<float>(std::rand() % (900 - 100));

        if (std::rand() % 2 == 0)
        {
            position = { 0.f, y };
            velocity = { 120.f, 0.f };
        }
        else
        {
            position = { 1200.f, y };
            velocity = { -120.f, 0.f };
        }

        // ===== Bottom Hull =====
        hull.setPointCount(6);

        hull.setPoint(0, { -32.f, 0.f });
        hull.setPoint(1, { -20.f, 6.f });
        hull.setPoint(2, { 20.f, 6.f });
        hull.setPoint(3, { 32.f, 0.f });
        hull.setPoint(4, { 20.f, -4.f });
        hull.setPoint(5, { -20.f, -4.f });

        hull.setFillColor(sf::Color::Transparent);
        hull.setOutlineColor(sf::Color::White);
        hull.setOutlineThickness(2.5f);


        // ===== Top Dome =====
        dome.setPointCount(4);

        dome.setPoint(0, { -12.f, -4.f });
        dome.setPoint(1, { -6.f, -12.f });
        dome.setPoint(2, { 6.f, -12.f });
        dome.setPoint(3, { 12.f, -4.f });

        dome.setFillColor(sf::Color::Transparent);
        dome.setOutlineColor(sf::Color::White);
        dome.setOutlineThickness(2.5f);

        hull.setScale(1.3f, 1.3f);
        dome.setScale(1.3f, 1.3f);

        if (type == UFOType::Small)
        {
            hull.setScale(0.7f, 0.7f);
            dome.setScale(0.7f, 0.7f);
        }

        baseY = position.y;
    }

    sf::ConvexShape& getShape()
    {
        return hull;
    }

    void update(float dt) override;

    void render(sf::RenderWindow& window) override
    {
        window.draw(hull);
        window.draw(dome);
    }

    UFOType type;

private:
    Game& game;

    float shootTimer = 0.f;
    float shootInterval = 1.5f; // seconds between shots

    float zigzagTimer = 0.f;
    float zigzagAmplitude = 120.f;
    float zigzagFrequency = 3.f;
    float zigzagChangeTimer = 0.f;
    float baseY = 0.f;

    sf::ConvexShape hull;
    sf::ConvexShape dome;

    sf::ConvexShape shape;
};

// ================== PARTICLE =============================

class Particle : public Entity
{
private:
    sf::Vertex line[2];
    float lifetime;

public:
    Particle(sf::Vector2f pos)
    {
        position = pos;
        lifetime = 0.4f; // short life

        float angle = static_cast<float>(rand() % 360) * 3.14159f / 180.f;
        float speed = 200.f + static_cast<float>(rand() % 200);

        velocity = { std::cos(angle) * speed,
                     std::sin(angle) * speed };

        line[0].color = sf::Color::White;
        line[1].color = sf::Color::White;

        line[0].position = position;
        line[1].position = position + sf::Vector2f(2.f, 2.f);
    }

    void update(float dt) override
    {
        lifetime -= dt;
        if (lifetime <= 0.f)
            isAlive = false;

        position += velocity * dt;

        line[0].position = position;
        line[1].position = position + sf::Vector2f(2.f, 2.f);
    }

    void render(sf::RenderWindow& window) override
    {
        window.draw(line, 2, sf::Lines);
    }
};

// =============== DEATH ANIMATION ====================

class ShipLine : public Entity
{
private:
    sf::RectangleShape rectangle;
    float lifetime;

public:
    ShipLine(sf::Vector2f p1,
        sf::Vector2f p2,
        sf::Vector2f shipVelocity)
    {
        lifetime = 2.f;

        // Direction of the original line segment
        sf::Vector2f diff = p2 - p1;

        float length = std::sqrt(diff.x * diff.x +
            diff.y * diff.y);

        // ===== Thickness HERE =====
        float thickness = 2.5f;   // change this to 6.f or 8.f if you want thicker

        rectangle.setSize({ length, thickness });
        rectangle.setFillColor(sf::Color::White);

        // Center vertically so rotation looks correct
        rectangle.setOrigin(0.f, thickness / 2.f);

        // Angle of the line
        float angle =
            std::atan2(diff.y, diff.x) * 180.f / PI;

        rectangle.setPosition(p1);
        rectangle.setRotation(angle);

        // ===== Explosion Drift (slow + subtle) =====
        float angleRad =
            static_cast<float>(std::rand()) / RAND_MAX
            * 2.f * PI;

        float speed =
            15.f + static_cast<float>(std::rand()) / RAND_MAX * 10.f;

        velocity.x = std::cos(angleRad) * speed;
        velocity.y = std::sin(angleRad) * speed;
    }

    void update(float dt) override
    {
        lifetime -= dt;

        if (lifetime <= 0.f)
            isAlive = false;

        rectangle.move(velocity * dt);
    }

    void render(sf::RenderWindow& window) override
    {
        window.draw(rectangle);
    }
};

// ===================== GAME =====================

class Game
{
public:
    Game()
        : window(sf::VideoMode(1200, 900), "Asteroids",
            sf::Style::Close | sf::Style::Titlebar)
    {
        loadHighScore();

        window.setKeyRepeatEnabled(false);  // IMPORTANT FIX

        font.loadFromFile("PressStart2P.ttf");

        highScoreText.setFont(font);
        highScoreText.setCharacterSize(18);
        highScoreText.setFillColor(sf::Color::White);
        highScoreText.setPosition(20.f, 80.f);

        scoreText.setFont(font);
        scoreText.setCharacterSize(18);
        scoreText.setFillColor(sf::Color::White);
        scoreText.setPosition(20.f, 20.f);

        livesText.setFont(font);
        livesText.setCharacterSize(18);
        livesText.setFillColor(sf::Color::White);
        livesText.setPosition(20.f, 50.f);

        titleText.setFont(font);
        titleText.setCharacterSize(64);
        titleText.setString("ASTEROIDS");
        sf::FloatRect bounds = titleText.getLocalBounds();
        titleText.setOrigin(bounds.left + bounds.width / 2.f,
            bounds.top + bounds.height / 2.f);
        titleText.setPosition(600.f, 200.f);

        coinText.setFont(font);
        coinText.setCharacterSize(24);
        coinText.setPosition(420.f, 400.f);

        startText.setFont(font);
        startText.setCharacterSize(20);
        startText.setPosition(380.f, 500.f);

        gameOverText.setFont(font);
        gameOverText.setCharacterSize(70);
        gameOverText.setFillColor(sf::Color::White);
        gameOverText.setString("GAME OVER");
        sf::FloatRect goBounds = gameOverText.getLocalBounds();
        gameOverText.setOrigin(goBounds.width / 2.f, goBounds.height / 2.f);
        gameOverText.setPosition(600.f, 350.f);

        restartText.setFont(font);
        restartText.setCharacterSize(30);
        restartText.setFillColor(sf::Color::White);
        restartText.setString("PRESS ENTER");
        restartText.setPosition(440.f, 450.f);

        //======AUDIO=====
        fireSound.setBuffer(fireBuffer);
        fireSound.setVolume(100.f);
        

        if (!fireBuffer.loadFromFile("fire.wav"))
        {
            std::cout << "Failed to load fire.wav\n";
        }

        bangLargeBuffer.loadFromFile("bangLarge.wav");
        bangMediumBuffer.loadFromFile("bangMedium.wav");
        bangSmallBuffer.loadFromFile("bangSmall.wav");

        bangLarge.setBuffer(bangLargeBuffer);
        bangMedium.setBuffer(bangMediumBuffer);
        bangSmall.setBuffer(bangSmallBuffer);

        if (!thrustBuffer.loadFromFile("thrust.wav"))
        {
            std::cout << "Error loading thrust.wav\n";
        }

        thrust.setBuffer(thrustBuffer);
        thrust.setLoop(true);   // engine should loop while thrusting
        thrust.setVolume(1000.f);

        saucerBigBuffer.loadFromFile("saucerBig.wav");
        saucerSmallBuffer.loadFromFile("saucerSmall.wav");

        saucerBigSound.setBuffer(saucerBigBuffer);
        saucerSmallSound.setBuffer(saucerSmallBuffer);

        saucerBigSound.setLoop(true);
        saucerSmallSound.setLoop(true);

        extraShipBuffer.loadFromFile("extraShip.wav");
        extraShipSound.setBuffer(extraShipBuffer);
        extraShipSound.setVolume(180.f);

        coinBuffer.loadFromFile("Insert Coin.mp3");
        coinSound.setBuffer(coinBuffer);

        for (int i = 0; i < 5; i++)
        {
            float x = static_cast<float>(std::rand() % 1200);
            float y = static_cast<float>(std::rand() % 900);

            sf::Vector2f spawnPos(x, y);

            entities.push_back(
                std::make_unique<Asteroid>(
                    spawnPos,
                    Asteroid::AsteroidSize::Large, currentWave
                )
            );
        }
    }

    sf::Sound& getThrustSound()
    {
        return thrust;
    }

    void loadHighScore();
    void saveHighScore();

    float waveDelay = 2.0f;
    float waveTimer = 0.f;
    bool betweenWaves = false;
    int currentWave = 1;

    Player* getPlayer()
    {
        for (auto& e : entities)
        {
            Player* player = dynamic_cast<Player*>(e.get());
            if (player && player->isAlive)
                return player;
        }

        return nullptr;
    }

    void run()
    {
        sf::Clock clock;

        while (window.isOpen())
        {
            float dt = clock.restart().asSeconds();

            handleEvents();
            update(dt);
            render();
        }
    }

    void spawn(std::unique_ptr<Entity> entity)
    {
        pendingEntities.push_back(std::move(entity));
    }

    void playerDied()
    {
        lives--;

        if (lives <= 0)
        {
            if (score > highScore)
            {
                state = GameState::EnteringInitials;
                enteringInitials = true;
                playerInitials.clear();   // better reset
            }

            else
            {
                gameOver = true;
                enteringInitials = true;
                gameOverTimer = 3.f;

                saucerBigSound.stop();
                saucerSmallSound.stop();
                thrust.stop();
            }
        }
        else
        {
            waitingForRespawn = true;
            respawnTimer = 3.0f;
        }
    }

    int getBulletCount() const
    {
        int count = 0;

        for (const auto& e : entities)
        {
            if (dynamic_cast<Bullet*>(e.get()))
                count++;
        }

        return count;
    }

    void addHighScore(std::string name, int playerScore)
    {
        highScores.push_back({ name, playerScore });

        std::sort(highScores.begin(), highScores.end(),
            [](const HighScore& a, const HighScore& b)
            {
                return a.score > b.score;
            });

        if (highScores.size() > 5)
            highScores.resize(5);
    }

    void playFireSound()
    {
        fireSound.play();
    }

    void playAsteroidSound(Asteroid::AsteroidSize size)
    {
        if (size == Asteroid::AsteroidSize::Large)
            bangLarge.play();
        else if (size == Asteroid::AsteroidSize::Medium)
            bangMedium.play();
        else
            bangSmall.play();
    }

    sf::SoundBuffer thrustBuffer;
    sf::Sound thrust;

private:
    enum class GameState
    {
        StartScreen,
        Playing,
        GameOver,
        EnteringInitials
    };

    bool ufoExists()
    {
        for (auto& e : entities)
        {
            UFO* ufo = dynamic_cast<UFO*>(e.get());
            if (ufo && ufo->isAlive)
                return true;
        }
        return false;
    }

    sf::SoundBuffer fireBuffer;
    sf::Sound fireSound;

    sf::SoundBuffer bangLargeBuffer;
    sf::SoundBuffer bangMediumBuffer;
    sf::SoundBuffer bangSmallBuffer;

    sf::Sound bangLarge;
    sf::Sound bangMedium;
    sf::Sound bangSmall;

    sf::SoundBuffer saucerBigBuffer;
    sf::SoundBuffer saucerSmallBuffer;

    sf::Sound saucerBigSound;
    sf::Sound saucerSmallSound;

    sf::SoundBuffer extraShipBuffer;
    sf::Sound extraShipSound;

    sf::SoundBuffer coinBuffer;
    sf::Sound coinSound;

    sf::Text gameOverText;
    sf::Text restartText;
    sf::Text initialsText;

    float ufoSpawnTimer = 0.f;
    float ufoSpawnDelay = 20.f;

    int coinsInserted = 0;
    const int coinsRequired = 1;

    sf::Text titleText;
    sf::Text coinText;
    sf::Text startText;

    GameState state = GameState::StartScreen;

    int nextExtraLifeScore = 10000;

    sf::Text highScoreText;

    float respawnTimer = 0.f;
    bool waitingForRespawn = false;

    sf::Text livesText;
    sf::Font font;
    sf::Text scoreText;

    int score = 0;
    int highScore = 0;

    std::vector<HighScore> highScores =
    {
        {"AAA",80000},
        {"BBB",40000},
        {"CCC",10000},
        {"DDD",4000},
        {"EEE",1000}
    };

    int lives = 3;
    bool gameOver = false;

    std::string playerInitials = "";
    bool enteringInitials = false;

    float gameOverTimer = 0.f;

    float gameOverBlinkTimer = 0.f;
    bool showGameOverText = true;


    void handleEvents()
    {
        sf::Event event;

        while (window.pollEvent(event))
        {
            // ================= INITIALS INPUT =================
            if (enteringInitials)
            {
                if (event.type == sf::Event::TextEntered)
                {
                    char c = static_cast<char>(event.text.unicode);

                    if (c >= 'a' && c <= 'z')
                        c = std::toupper(c);

                    if (c >= 'A' && c <= 'Z')
                    {
                        if (playerInitials.size() < 3)
                            playerInitials += c;
                    }

                    if (event.text.unicode == 8 && !playerInitials.empty())
                        playerInitials.pop_back();
                }

                if (event.type == sf::Event::KeyPressed &&
                    event.key.code == sf::Keyboard::Enter)
                {
                    if (playerInitials.size() == 3)
                    {
                        addHighScore(playerInitials, score);
                        saveHighScore();
                    }

                    enteringInitials = false;
                    gameOver = true;
                }

                continue;
            }

            if (event.type == sf::Event::Closed)
                window.close();

            if (event.type == sf::Event::KeyPressed)
            {
                if (state == GameState::StartScreen)
                {
                    if (event.key.code == sf::Keyboard::C)
                    {
                        coinsInserted++;
                        coinSound.play();
                    }

                    if (event.key.code == sf::Keyboard::Enter &&
                        coinsInserted >= coinsRequired)
                    {
                        coinsInserted -= coinsRequired;   // consume coin

                        resetGame();                      // reset everything
                        state = GameState::Playing;

                        entities.push_back(std::make_unique<Player>(*this));
                        spawnWave();
                    }
                }

                for (auto& e : entities)
                    e->handleEvent(event);
            }

            if (state == GameState::Playing)
            {
                for (auto& e : entities)
                    e->handleEvent(event);
            }
        }
    }

    void resetGame()
    {
        entities.clear();
        pendingEntities.clear();

        saucerSmallSound.stop();
        saucerBigSound.stop();

        score = 0;
        lives = 3;
        currentWave = 1;
        nextExtraLifeScore = 10000;

        gameOver = false;
        waitingForRespawn = false;
        betweenWaves = false;
    }

    void update(float dt)
    {
        coinText.setString("Insert Coin: " + std::to_string(coinsInserted) + "/" + std::to_string(coinsRequired));

        if (gameOver && !enteringInitials)
        {
            gameOverBlinkTimer += dt;

            if (gameOverBlinkTimer > 0.5f)
            {
                showGameOverText = !showGameOverText;
                gameOverBlinkTimer = 0.f;
            }

            gameOverTimer -= dt;

            if (gameOverTimer <= 0.f)
            {
                resetGame();
                state = GameState::StartScreen;
            }
        }

        if (coinsInserted >= coinsRequired)
        {
            startText.setString("PRESS ENTER TO START");
        }
        else
        {
            startText.setString("PRESS C TO INSERT COIN");
        }

        sf::FloatRect coinBounds = coinText.getLocalBounds();
        coinText.setOrigin(coinBounds.left + coinBounds.width / 2.f,
            coinBounds.top + coinBounds.height / 2.f);
        coinText.setPosition(600.f, 400.f);

        sf::FloatRect startBounds = startText.getLocalBounds();
        startText.setOrigin(startBounds.left + startBounds.width / 2.f,
            startBounds.top + startBounds.height / 2.f);
        startText.setPosition(600.f, 500.f);

        static float blinkTimer = 0.f;
        blinkTimer += dt;

        if ((int)(blinkTimer * 2) % 2 == 0)
            coinText.setFillColor(sf::Color::White);
        else
            coinText.setFillColor(sf::Color::Transparent);

            for (auto& e : entities)
                e->update(dt);

        bool asteroidExists = false;

        for (auto& e : entities)
        {
            if (dynamic_cast<Asteroid*>(e.get()))
            {
                asteroidExists = true;
                break;
            }
        }

        if (!ufoExists() && asteroidExists)
        {
            ufoSpawnTimer += dt;
        }

        if (ufoSpawnTimer >= ufoSpawnDelay)
        {
            ufoSpawnTimer = 0.f;

            ufoSpawnDelay = std::max(8.f, 20.f - currentWave * 1.5f);

            float smallChance = std::min(0.1f + currentWave * 0.08f, 0.8f);

            UFO::UFOType type;

            if ((std::rand() / (float)RAND_MAX) < smallChance)
                type = UFO::UFOType::Small;
            else
                type = UFO::UFOType::Large;

            entities.push_back(std::make_unique<UFO>(*this, type));

            if (type == UFO::UFOType::Large)
                saucerBigSound.play();
            else
                saucerSmallSound.play();
        }

        highScoreText.setString("High: " + std::to_string(highScore));

        if (gameOver)
            return;

        // ================= COLLISION CHECK =================

        for (auto& a : entities)
        {
            Bullet* bullet = dynamic_cast<Bullet*>(a.get());
            if (!bullet || !bullet->isAlive)
                continue;

            for (auto& b : entities)
            {
                UFO* ufo = dynamic_cast<UFO*>(b.get());
                if (ufo && ufo->isAlive)
                {
                    if (bullet->fromUFO)
                        continue; // UFOs ignore their own bullets

                    if (bullet->getShape().getGlobalBounds().intersects(
                        ufo->getShape().getGlobalBounds()))
                    {
                        bullet->isAlive = false;
                        ufo->isAlive = false;

                        saucerBigSound.stop();
                        saucerSmallSound.stop();
                        bangMedium.play();

                        score += 1000;

                        for (int i = 0; i < 20; i++)
                        {
                            pendingEntities.push_back(
                                std::make_unique<Particle>(ufo->getPosition())
                            );
                        }

                        break;
                    }
                }

                Asteroid* asteroid = dynamic_cast<Asteroid*>(b.get());
                if (!asteroid || !asteroid->isAlive)
                    continue;

                if (bullet->getShape().getGlobalBounds().intersects(
                    asteroid->getShape().getGlobalBounds()))
                {
                    bullet->isAlive = false;

                    Asteroid::AsteroidSize currentSize = asteroid->getSize();

                    playAsteroidSound(currentSize);

                    if (currentSize == Asteroid::AsteroidSize::Large)
                        score += 20;
                    else if (currentSize == Asteroid::AsteroidSize::Medium)
                        score += 50;
                    else
                        score += 100;

                    sf::Vector2f pos = asteroid->getPosition();
                    sf::Vector2f originalVelocity = asteroid->getVelocity();

                    asteroid->isAlive = false;

                    // spawn particles
                    for (int i = 0; i < 15; i++)
                    {
                        pendingEntities.push_back(
                            std::make_unique<Particle>(pos)
                        );
                    }

                    // ===== SPLITTING =====
                    if (currentSize == Asteroid::AsteroidSize::Large)
                    {
                        float spread = 30.f;

                        auto a1 = std::make_unique<Asteroid>(
                            pos + sf::Vector2f(spread, 0.f),
                            Asteroid::AsteroidSize::Medium, currentWave);

                        auto a2 = std::make_unique<Asteroid>(
                            pos + sf::Vector2f(-spread, 0.f),
                            Asteroid::AsteroidSize::Medium, currentWave);

                        // Push them apart
                        a1->setVelocity({ -originalVelocity.y, originalVelocity.x });
                        a2->setVelocity({ originalVelocity.y, -originalVelocity.x });

                        pendingEntities.push_back(std::move(a1));
                        pendingEntities.push_back(std::move(a2));
                    }
                    else if (currentSize == Asteroid::AsteroidSize::Medium)
                    {
                        float spread = 20.f;

                        auto a1 = std::make_unique<Asteroid>(
                            pos + sf::Vector2f(spread, 0.f),
                            Asteroid::AsteroidSize::Small, currentWave);

                        auto a2 = std::make_unique<Asteroid>(
                            pos + sf::Vector2f(-spread, 0.f),
                            Asteroid::AsteroidSize::Small, currentWave);

                        a1->setVelocity({ -originalVelocity.y, originalVelocity.x });
                        a2->setVelocity({ originalVelocity.y, -originalVelocity.x });

                        pendingEntities.push_back(std::move(a1));
                        pendingEntities.push_back(std::move(a2));
                    }
                    // spawn particles
                    for (int i = 0; i < 15; i++)
                    {
                        pendingEntities.push_back(
                            std::make_unique<Particle>(asteroid->position)
                        );
                    }
                }
            }
        }

        // ----- ASTEROID vs ASTEROID COLLISION -----
        for (size_t i = 0; i < entities.size(); i++)
        {
            Asteroid* a = dynamic_cast<Asteroid*>(entities[i].get());
            if (!a || !a->isAlive) continue;

            for (size_t j = i + 1; j < entities.size(); j++)
            {
                Asteroid* b = dynamic_cast<Asteroid*>(entities[j].get());
                if (!b || !b->isAlive) continue;

                sf::Vector2f diff = a->getPosition() - b->getPosition();
                float distance = std::sqrt(diff.x * diff.x + diff.y * diff.y);

                if (distance < a->getRadius() + b->getRadius())
                {
                    sf::Vector2f diff = a->getPosition() - b->getPosition();
                    float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);

                    if (dist == 0) dist = 0.1f;

                    sf::Vector2f normal = diff / dist;

                    float overlap = (a->getRadius() + b->getRadius()) - dist;

                    // Push them apart
                    a->position += normal * (overlap / 2.f);
                    b->position -= normal * (overlap / 2.f);

                    // Swap velocity
                    sf::Vector2f temp = a->getVelocity();
                    a->setVelocity(b->getVelocity());
                    b->setVelocity(temp);
                }
            }
        }
        Player* player = nullptr;

        // Find the player
        for (auto& e : entities)
        {
            player = dynamic_cast<Player*>(e.get());
            if (player) break;
        }

        if (player && player->isAlive)
        {
            auto shipPoints = player->getWorldPoints();

            for (auto& e : entities)
            {
                // ===== ASTEROID COLLISION =====
                Asteroid* asteroid = dynamic_cast<Asteroid*>(e.get());

                if (asteroid && asteroid->isAlive)
                {
                    if (!player->isInvincible() &&
                        polygonCircleCollision(
                            shipPoints,
                            asteroid->getPosition(),
                            asteroid->getRadius()))
                    {
                        player->explode();
                        playerDied();

                        thrust.stop();

                        Asteroid::AsteroidSize currentSize = asteroid->getSize();

                        playAsteroidSound(currentSize);

                        if (currentSize == Asteroid::AsteroidSize::Large)
                            score += 20;
                        else if (currentSize == Asteroid::AsteroidSize::Medium)
                            score += 50;
                        else
                            score += 100;

                        sf::Vector2f pos = asteroid->getPosition();
                        sf::Vector2f originalVelocity = asteroid->getVelocity();

                        asteroid->isAlive = false;

                        // spawn particles
                        for (int i = 0; i < 15; i++)
                        {
                            pendingEntities.push_back(
                                std::make_unique<Particle>(pos)
                            );
                        }

                        // ===== SPLITTING =====
                        if (currentSize == Asteroid::AsteroidSize::Large)
                        {
                            float spread = 30.f;

                            auto a1 = std::make_unique<Asteroid>(
                                pos + sf::Vector2f(spread, 0.f),
                                Asteroid::AsteroidSize::Medium, currentWave);

                            auto a2 = std::make_unique<Asteroid>(
                                pos + sf::Vector2f(-spread, 0.f),
                                Asteroid::AsteroidSize::Medium, currentWave);

                            // Push them apart
                            a1->setVelocity({ -originalVelocity.y, originalVelocity.x });
                            a2->setVelocity({ originalVelocity.y, -originalVelocity.x });

                            pendingEntities.push_back(std::move(a1));
                            pendingEntities.push_back(std::move(a2));
                        }
                        else if (currentSize == Asteroid::AsteroidSize::Medium)
                        {
                            float spread = 20.f;

                            auto a1 = std::make_unique<Asteroid>(
                                pos + sf::Vector2f(spread, 0.f),
                                Asteroid::AsteroidSize::Small, currentWave);

                            auto a2 = std::make_unique<Asteroid>(
                                pos + sf::Vector2f(-spread, 0.f),
                                Asteroid::AsteroidSize::Small, currentWave);

                            a1->setVelocity({ -originalVelocity.y, originalVelocity.x });
                            a2->setVelocity({ originalVelocity.y, -originalVelocity.x });

                            pendingEntities.push_back(std::move(a1));
                            pendingEntities.push_back(std::move(a2));
                        }
                    }
                }


                // ===== UFO COLLISION =====
                UFO* ufo = dynamic_cast<UFO*>(e.get());

                if (ufo && ufo->isAlive)
                {
                    if (!player->isInvincible() &&
                        ufo->getShape().getGlobalBounds().intersects(
                            player->getShape().getGlobalBounds()))
                    {
                        ufo->isAlive = false;

                        saucerBigSound.stop();
                        saucerSmallSound.stop();
                        bangMedium.play();

                        if (ufo->type == UFO::UFOType::Large)
                            score += 200;
                        else
                            score += 1000;

                        player->explode();
                        playerDied();

                        thrust.stop();
                    }
                }

                //========PLAYER VS UFO COLLISION=========
                Bullet* bullet = dynamic_cast<Bullet*>(e.get());

                if (bullet && bullet->isAlive && bullet->fromUFO)
                {
                    if (!player->isInvincible() &&
                        bullet->getShape().getGlobalBounds().intersects(
                            player->getShape().getGlobalBounds()))
                    {
                        bullet->isAlive = false;

                        bangMedium.play();

                        player->explode();
                        playerDied();

                        thrust.stop();
                    }
                }
            }
        }

        for (auto& e : pendingEntities)
            entities.push_back(std::move(e));

        pendingEntities.clear();

        // Dead entity removal
        entities.erase(
            std::remove_if(entities.begin(), entities.end(),
                [](const std::unique_ptr<Entity>& e)
                {
                    return !e->isAlive;
                }),
            entities.end()
        );

        // ===== WAVE SYSTEM =====

// Check if any asteroids remain
        bool asteroidForUFO = false;

        for (auto& e : entities)
        {
            if (dynamic_cast<Asteroid*>(e.get()))
            {
                asteroidForUFO = true;
                break;
            }
        }

        if (!ufoExists() && asteroidForUFO)
        {
            ufoSpawnTimer += dt;
        }

        // If no asteroids and not already waiting
        if (!asteroidExists && !betweenWaves)
        {
            betweenWaves = true;
            waveTimer = waveDelay;
        }

        // Handle countdown
        if (betweenWaves)
        {
            waveTimer -= dt;

            if (waveTimer <= 0.f)
            {
                currentWave++;
                spawnWave();
                betweenWaves = false; // IMPORTANT: reset immediately
            }
        }

        if (waitingForRespawn)
        {
            respawnTimer -= dt;

            if (respawnTimer <= 0.f)
            {
                if (state == GameState::Playing)
                {
                    entities.push_back(std::make_unique<Player>(*this));
                }
                waitingForRespawn = false;
            }
        }

        bool playerExists = false;

        for (auto& e : entities)
        {
            if (dynamic_cast<Player*>(e.get()))
            {
                playerExists = true;
                break;
            }
        }

        if (!playerExists && lives > 0 && !gameOver && !waitingForRespawn)
        {
            if (state == GameState::Playing)
            {
                entities.push_back(std::make_unique<Player>(*this));
            }
        }

        // ===== EXTRA LIFE SYSTEM =====
        if (score >= nextExtraLifeScore)
        {
            if (lives < 9)
                if (score >= nextExtraLifeScore)
                lives++;

            nextExtraLifeScore += 10000;
        }

        if (score > highScore)
        {
            highScore = score;
        }

        scoreText.setString("Score: " + std::to_string(score));
        livesText.setString("Lives: " + std::to_string(lives));
    }

    void render()
    {
        window.clear(sf::Color::Black);

        // draw asteroids / particles / everything
        for (auto& e : entities)
            e->render(window);

        if (!enteringInitials && state == GameState::StartScreen && !gameOver)
        {
            window.draw(titleText);
            window.draw(coinText);
            window.draw(startText);
        }

        if (!enteringInitials && gameOver && showGameOverText)
        {
            window.draw(gameOverText);
        }

        window.draw(scoreText);
        window.draw(livesText);

        if (enteringInitials)
        {
            sf::Text initialsText;
            initialsText.setFont(font);
            initialsText.setCharacterSize(40);
            initialsText.setFillColor(sf::Color::White);

            initialsText.setString("NEW HIGH SCORE!\nENTER INITIALS:\n" + playerInitials);
            initialsText.setPosition(300, 300);

            window.draw(initialsText);
            
            float y = 120.f;

            for (auto& hs : highScores)
            {
                sf::Text t;
                t.setFont(font);
                t.setCharacterSize(18);
                t.setFillColor(sf::Color::White);

                t.setString(hs.name + "  " + std::to_string(hs.score));
                t.setPosition(20.f, y);

                window.draw(t);

                y += 30.f;
            }
        }

        float y = 120.f;

        if (state != GameState::Playing)
        {
            for (auto& hs : highScores)
            {
                sf::Text t;
                t.setFont(font);
                t.setCharacterSize(18);
                t.setFillColor(sf::Color::White);

                t.setString(hs.name + "  " + std::to_string(hs.score));
                t.setPosition(20.f, y);

                window.draw(t);

                y += 30.f;
            }
        }
        window.display();
    }

    void spawnWave()
    {
        int asteroidCount = 3 + currentWave; // increase number each wave

        sf::Vector2f playerPos;

        for (auto& e : entities)
        {
            Player* p = dynamic_cast<Player*>(e.get());
            if (p)
            {
                playerPos = p->getPosition();
                break;
            }
        }

        for (int i = 0; i < asteroidCount; i++)
        {
            sf::Vector2f spawnPos;

            do
            {
                float x = static_cast<float>(std::rand() % 1200);
                float y = static_cast<float>(std::rand() % 900);

                spawnPos = { x, y };

            } while (distance(spawnPos, playerPos) < 250.f);

            entities.push_back(
                std::make_unique<Asteroid>(
                    spawnPos,
                    Asteroid::AsteroidSize::Large,
                    currentWave
                )
            );
        }
    }

    sf::RenderWindow window;
    std::vector<std::unique_ptr<Entity>> entities;
    std::vector<std::unique_ptr<Entity>> pendingEntities;
};

// ===================== PLAYER IMPLEMENTATION =====================

Player::Player(Game& g)
    : Entity({ 600.f, 450.f }, 0.f), game(g)
{
    invincible = true;
    invincibleTimer = 2.0f;

    // Ship shape
    ship.setPointCount(4);
    ship.setPoint(0, { 10.f, 0.f });
    ship.setPoint(1, { -20.f, -12.5f });
    ship.setPoint(2, { -10.f, 0.f });
    ship.setPoint(3, { -20.f, 12.5f });

    ship.setFillColor(sf::Color::Transparent);
    ship.setOutlineColor(sf::Color::White);
    ship.setOutlineThickness(2.f);

    // Thruster (narrow & proportional)
    sf::Vector2f nose = ship.getPoint(0);
    sf::Vector2f backTop = ship.getPoint(1);
    sf::Vector2f backBottom = ship.getPoint(3);

    sf::Vector2f rearCenter(
        (backTop.x + backBottom.x) / 2.f,
        (backTop.y + backBottom.y) / 2.f
    );

    float shipLength = nose.x - rearCenter.x;
    float flameLength = shipLength * 0.6f;

    float rearHeight = backBottom.y - backTop.y;
    float flameHalfWidth = rearHeight * 0.15f;

    thruster.setPointCount(3);
    thruster.setPoint(0, { rearCenter.x, rearCenter.y - flameHalfWidth });
    thruster.setPoint(1, { rearCenter.x - flameLength, rearCenter.y });
    thruster.setPoint(2, { rearCenter.x, rearCenter.y + flameHalfWidth });

    thruster.setFillColor(sf::Color::Transparent);
    thruster.setOutlineColor(sf::Color::White);
    thruster.setOutlineThickness(2.f);
}

void Player::handleEvent(const sf::Event& event)
{
    if (event.type == sf::Event::KeyPressed &&
        event.key.code == sf::Keyboard::Space)
    {
        if (shootTimer <= 0.f && game.getBulletCount() < MAX_BULLETS)
        {
            shootTimer = SHOOT_DELAY;

            float rad = angle * (PI / 180.f);
            sf::Vector2f dir(std::cos(rad), std::sin(rad));

            float noseOffset = ship.getPoint(0).x;
            sf::Vector2f spawnPos = position + dir * noseOffset;

            game.spawn(std::make_unique<Bullet>(spawnPos, dir));
            game.playFireSound();
        }
    }

    if (event.type == sf::Event::KeyPressed &&
        event.key.code == sf::Keyboard::H)
    {
        if (hyperspaceCooldown <= 0.f)
        {
            hyperspaceCooldown = 1.0f; // 1 second cooldown

            int chance = std::rand() % 100;

            // 10% chance to explode
            if (chance < 10)
            {
                explode();
                game.playerDied();
                return;
            }

            // teleport to random location
            position.x = static_cast<float>(std::rand() % 1200);
            position.y = static_cast<float>(std::rand() % 900);

            velocity = { 0.f, 0.f };
        }
    }
}

void Player::update(float dt)
{
    hyperspaceCooldown -= dt;

    // ----- INVINCIBILITY TIMER -----
    if (invincible)
    {
        invincibleTimer -= dt;

        if (invincibleTimer <= 0.f)
            invincible = false;
    }
    
    shootTimer -= dt;
    isThrusting = false;

    // ----- ROTATION -----
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left))
    {
        angle -= TURN_SPEED * dt;
    }

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right))
    {
        angle += TURN_SPEED * dt;
    }

    // ------THRUST------
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up))
    {
        isThrusting = true;

        if (game.getThrustSound().getStatus() != sf::Sound::Playing)
            game.getThrustSound().play();

        float rad = angle * (PI / 180.f);
        sf::Vector2f forward(std::cos(rad), std::sin(rad));

        velocity += forward * ACCELERATION * dt;

        flickerTimer += dt;
        if (flickerTimer > 0.05f)
        {
            flickerTimer = 0.f;
            flickerVisible = !flickerVisible;
        }
    }
    else
    {
        game.getThrustSound().stop();   // stop engine sound
        flickerTimer = 0.f;
        flickerVisible = true;
    }

    velocity *= DRAG;

    float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
    if (speed > MAX_SPEED)
        velocity = (velocity / speed) * MAX_SPEED;

    position += velocity * dt;

    // --- Screen Wrapping ---
    if (position.x > 1200.f)
        position.x = 0.f;
    else if (position.x < 0.f)
        position.x = 1200.f;

    if (position.y > 900.f)
        position.y = 0.f;
    else if (position.y < 0.f)
        position.y = 900.f;
}

void Player::render(sf::RenderWindow& window)
{
    ship.setPosition(position);
    ship.setRotation(angle);

    thruster.setPosition(position);
    thruster.setRotation(angle);

    bool visible = true;

    if (invincible)
    {
        // Blink effect
        visible = static_cast<int>(invincibleTimer * 10) % 2 == 0;
    }

    if (visible)
    {
        if (isThrusting && flickerVisible)
            window.draw(thruster);

        window.draw(ship);
    }
}

void Player::explode()
{

    ship.setPosition(position);   
    ship.setRotation(angle);      

    std::vector<sf::Vector2f> points;

    for (size_t i = 0; i < ship.getPointCount(); i++)
    {
        sf::Vector2f p =
            ship.getTransform().transformPoint(ship.getPoint(i));

        points.push_back(p);
    }

    for (size_t i = 0; i < points.size(); i++)
    {
        sf::Vector2f p1 = points[i];
        sf::Vector2f p2 = points[(i + 1) % points.size()];

        game.spawn(
            std::make_unique<ShipLine>(p1, p2, velocity)
        );
    }

    isAlive = false;
}

void UFO::update(float dt)
{
    zigzagTimer += dt;

    zigzagChangeTimer += dt;

    if (zigzagChangeTimer > 2.5f)
    {
        zigzagChangeTimer = 0.f;

        // shift the center line randomly
        baseY += (std::rand() % 200 - 100);

        baseY = std::clamp(baseY, 100.f, 800.f);
    }

    position.x += velocity.x * dt;
    position.y = baseY + std::sin(zigzagTimer * zigzagFrequency) * zigzagAmplitude;

    if (position.x > 1200.f)
        position.x = 0.f;
    else if (position.x < 0.f)
        position.x = 1200.f;

    if (position.y > 900.f)
        position.y = 0.f;
    else if (position.y < 0.f)
        position.y = 900.f;

    hull.setPosition(position);
    dome.setPosition(position);

    shootTimer += dt;

    if (shootTimer >= shootInterval)
    {
        shootTimer = 0.f;

        sf::Vector2f dir;

        if (type == UFOType::Small)
        {
            Player* player = game.getPlayer();

            if (player)
            {
                sf::Vector2f playerPos = player->getPosition();
                sf::Vector2f playerVel = player->getVelocity();

                float predictionTime = 0.3f + game.currentWave * 0.05f;
                predictionTime = std::min(predictionTime, 1.0f);

                sf::Vector2f predictedPos = playerPos + playerVel * predictionTime;

                sf::Vector2f toTarget = predictedPos - position;

                float angle = std::atan2(toTarget.y, toTarget.x);

                float spread = 0.35f - (game.currentWave * 0.02f);
                spread = std::max(spread, 0.08f);
                float error = ((float)std::rand() / RAND_MAX - 0.5f) * spread;

                angle += error;

                dir = { std::cos(angle), std::sin(angle) };
            }
            else
            {
                float angle = (std::rand() % 360) * PI / 180.f;
                dir = { std::cos(angle), std::sin(angle) };
            }
        }
        else
        {
            float angle = static_cast<float>(std::rand() % 360) * 3.14159f / 180.f;
            dir = { std::cos(angle), std::sin(angle) };
        }

        sf::Vector2f spawnPos = position + dir * 40.f;

        game.spawn(std::make_unique<Bullet>(spawnPos, dir, true));

        game.playFireSound();
    }
}

// ===================== MAIN =====================

int main()
{
    std::cout << "Running from: "
        << std::filesystem::current_path()
        << std::endl;

    std::srand(static_cast<unsigned>(std::time(nullptr)));

    Game game;
    game.run();
}

void Game::loadHighScore()
{
    std::ifstream file("highscore.txt");

    if (!file.is_open())
        return;

    highScores.clear();

    std::string name;
    int score;

    while (file >> name >> score)
    {
        highScores.push_back({ name, score });
    }

    file.close();

    if (!highScores.empty())
        highScore = highScores.front().score;
}

void Game::saveHighScore()
{
    std::ofstream file("highscore.txt");

    if (!file.is_open())
        return;

    for (const auto& hs : highScores)
    {
        file << hs.name << " " << hs.score << "\n";
    }

    file.close();
}
