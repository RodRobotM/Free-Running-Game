#define UNICODE
#define _UNICODE
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

struct Dot {
    double x;
    double y;
    double z;
    bool selected;
};

struct Platform {
    double x;
    double y;
    double z;
    double width;
    double depth;
    COLORREF color;
    bool floor;
};

constexpr double PI = 3.14159265358979323846;
constexpr double SPHERE_RADIUS = 220.0;
constexpr int CONTROL_HEIGHT = 82;
constexpr int MIN_EYE_DISTANCE = 20;
constexpr int MAX_EYE_DISTANCE = 700;
constexpr int DEFAULT_EYE_DISTANCE = 180;
constexpr double STANDARD_GRAVITY = 9.80665;
constexpr double FORCE_APPLICATION_TIME = 0.18;
constexpr double IMPACT_TIME = 0.18;
constexpr double PLATFORM_SIZE = 4.0;
constexpr int BUTTON_RESUME = 2001;
constexpr int BUTTON_RESTART = 2002;
constexpr int BUTTON_EXIT = 2003;
constexpr int BUTTON_MAP = 3001;
constexpr int BUTTON_GRAPHICS = 3002;
constexpr int BUTTON_PLAYER = 3003;
constexpr int BUTTON_FLIPS = 3004;
constexpr int BUTTON_PHYSICS = 3005;
constexpr int BUTTON_BACK = 3099;
constexpr int BUTTON_POINT_LIGHT = 3210;
constexpr int BUTTON_SHOW_FPS = 3220;

std::vector<Dot> dots;
std::vector<Platform> platforms;
double rotationX = 0.0;
double rotationY = 0.0;
int eyeDistance = DEFAULT_EYE_DISTANCE;
POINT lastMouse{};
bool dragging = false;
bool paused = false;
bool keyState[256]{};
bool grounded = true;
double playerX = 0.0;
double playerY = 4.0;
double playerZ = 0.0;
double playerHeight = 1.74;
double velocityX = 0.0;
double velocityZ = 0.0;
double verticalSpeed = 0.0;
double configuredAcceleration = 12.0;
double playerMass = 70.0;
double jumpForce = 120.0;
double receptionCapacity = 250.0;
double configuredGravity = STANDARD_GRAVITY;
double stopAcceleration = 18.0;
bool fallen = false;
double fallElapsed = 0.0;
double worldTime = 0.0;
int menuPage = 0;
int mapRadius = 3;
int platformDensity = 2;
int platformSize = 100;
int platformHeight = 100;
int platformSpacing = 100;
int starCount = 400;
bool pointLight = true;
bool showFps = false;
int fpsLimit = 60;
int fieldOfView = 60;
int configuredMoveSpeed = 7;
double mortalSpeed = 0.8;
double twistSpeed = 0.8;
enum class FlipKind { None, Front, Back, TwistLeft, TwistRight, SideLeft, SideRight };
int activeFlipMask = 0;
double flipElapsed = 0.0;
double flipDuration = 1.0;
double flipPitch = 0.0;
double flipYaw = 0.0;
double flipRoll = 0.0;
double lastMapCenterX = 0.0;
double lastMapCenterZ = 0.0;

double generatedPlatformHeight(int index) {
    static constexpr double levels[] = {0.0, 0.8, 1.8, 3.2, 4.8, 6.6, 2.6, 5.4};
    return levels[index % (sizeof(levels) / sizeof(levels[0]))] * platformHeight / 100.0;
}
int frameCounter = 0;
int displayedFps = 0;
ULONGLONG fpsWindowStart = 0;
ULONGLONG lastTick = 0;
HWND eyeDistanceSlider = nullptr;
HWND eyeDistanceLabel = nullptr;
HWND resumeButton = nullptr;
HWND restartButton = nullptr;
HWND exitButton = nullptr;
HWND mapButton = nullptr;
HWND graphicsButton = nullptr;
HWND playerButton = nullptr;
HWND flipsButton = nullptr;
HWND physicsButton = nullptr;
HWND backButton = nullptr;
HWND pointLightButton = nullptr;
HWND showFpsButton = nullptr;
HWND gameWindow = nullptr;

struct MenuOption {
    int page;
    int minusId;
    int plusId;
    HWND label;
    HWND slider;
};
std::vector<MenuOption> menuOptions;

struct ProjectedDot {
    POINT position;
    double depth;
    bool selected;
};

void resetGame() {
    platforms.clear();
    playerX = 0.0;
    playerY = 0.35 + playerHeight * 0.5;
    playerZ = 0.0;
    velocityX = 0.0;
    velocityZ = 0.0;
    verticalSpeed = 0.0;
    grounded = true;
    fallen = false;
    fallElapsed = 0.0;
    rotationX = 0.0;
    rotationY = 0.0;
    worldTime = 0.0;
    activeFlipMask = 0;
    flipElapsed = 0.0;
    flipPitch = 0.0;
    flipYaw = 0.0;
    flipRoll = 0.0;
    lastMapCenterX = playerX;
    lastMapCenterZ = playerZ;

    platforms.push_back({0.0, 0.0, 0.0, 8.0, 8.0, RGB(80, 210, 170), false});
    platforms.push_back({0.0, -3.0, 60.0, 200.0, 200.0, RGB(55, 60, 72), true});

    int platformIndex = 0;
    double spacing = 7.0 * platformSpacing / 100.0;
    double scale = platformSize / 100.0;
    for (int ring = 1; ring <= mapRadius * 3; ++ring) {
        int sectors = std::max(8, ring * 8 * platformDensity / 2);
        double radius = ring * spacing;
        for (int sector = 0; sector < sectors; ++sector) {
            double angle = 2.0 * PI * sector / sectors;
            double height = generatedPlatformHeight(platformIndex);
            platforms.push_back({std::cos(angle) * radius, height, std::sin(angle) * radius,
                PLATFORM_SIZE * scale, PLATFORM_SIZE * scale,
                platformIndex++ % 3 == 0 ? RGB(255, 110, 90) : RGB(80, 210, 170), false});
        }
    }
}

int flipMask(FlipKind kind) {
    return 1 << static_cast<int>(kind);
}

void startFlip(FlipKind kind) {
    if (activeFlipMask == 0) {
        flipElapsed = 0.0;
        double speed = kind == FlipKind::TwistLeft || kind == FlipKind::TwistRight
            ? twistSpeed : mortalSpeed;
        flipDuration = 1.0 / speed;
        verticalSpeed = std::max(verticalSpeed,
            jumpForce * 9.80665 / std::max(1.0, playerMass) * FORCE_APPLICATION_TIME);
        grounded = false;
    }
    activeFlipMask |= flipMask(kind);
}

void updateFlip(double deltaSeconds) {
    if (activeFlipMask == 0) return;
    flipElapsed += deltaSeconds;
    double progress = std::min(1.0, flipElapsed / flipDuration);
    double angle = 2.0 * PI * progress;
    flipPitch = 0.0;
    flipYaw = 0.0;
    flipRoll = 0.0;
    if (activeFlipMask & flipMask(FlipKind::Front)) flipPitch += angle;
    if (activeFlipMask & flipMask(FlipKind::Back)) flipPitch -= angle;
    if (activeFlipMask & flipMask(FlipKind::TwistLeft)) flipYaw -= angle;
    if (activeFlipMask & flipMask(FlipKind::TwistRight)) flipYaw += angle;
    if (activeFlipMask & flipMask(FlipKind::SideLeft)) flipRoll -= angle;
    if (activeFlipMask & flipMask(FlipKind::SideRight)) flipRoll += angle;
    if (progress >= 1.0) {
        activeFlipMask = 0;
        flipPitch = 0.0;
        flipYaw = 0.0;
        flipRoll = 0.0;
    }
}

void updateGame(double deltaSeconds) {
    if (paused) {
        return;
    }

    if (fallen) {
        fallElapsed += deltaSeconds;
        velocityX = 0.0;
        velocityZ = 0.0;
        rotationX = std::min(1.0, rotationX + deltaSeconds * 2.0);
        if (keyState[VK_SPACE] && fallElapsed >= 0.8) {
            fallen = false;
            fallElapsed = 0.0;
            grounded = true;
            rotationX = 0.0;
        }
        return;
    }

    double previousX = playerX;
    double previousZ = playerZ;
    double forward = (keyState['W'] ? 1.0 : 0.0) - (keyState['S'] ? 1.0 : 0.0);
    double strafe = (keyState['D'] ? 1.0 : 0.0) - (keyState['A'] ? 1.0 : 0.0);
    double length = std::sqrt(forward * forward + strafe * strafe);
    if (length > 0.0) {
        forward /= length;
        strafe /= length;
        double accelerationX = (std::sin(rotationY) * forward + std::cos(rotationY) * strafe) * configuredAcceleration;
        double accelerationZ = (std::cos(rotationY) * forward - std::sin(rotationY) * strafe) * configuredAcceleration;
        velocityX += accelerationX * deltaSeconds;
        velocityZ += accelerationZ * deltaSeconds;
    } else {
        double speed = std::hypot(velocityX, velocityZ);
        double reduction = std::min(speed, stopAcceleration * deltaSeconds);
        if (speed > 0.0) {
            velocityX -= velocityX / speed * reduction;
            velocityZ -= velocityZ / speed * reduction;
        }
    }
    double horizontalSpeed = std::hypot(velocityX, velocityZ);
    if (horizontalSpeed > configuredMoveSpeed) {
        double scale = configuredMoveSpeed / horizontalSpeed;
        velocityX *= scale;
        velocityZ *= scale;
    }
    playerX += velocityX * deltaSeconds;
    playerZ += velocityZ * deltaSeconds;

    if (keyState[VK_LEFT]) rotationY -= 1.8 * deltaSeconds;
    if (keyState[VK_RIGHT]) rotationY += 1.8 * deltaSeconds;
    if (keyState[VK_UP]) rotationX = std::max(-1.0, rotationX - 1.4 * deltaSeconds);
    if (keyState[VK_DOWN]) rotationX = std::min(1.0, rotationX + 1.4 * deltaSeconds);
    updateFlip(deltaSeconds);

    constexpr double PLAYER_RADIUS = 0.35;
    double playerBottom = playerY - playerHeight * 0.5;
    double playerTop = playerY + playerHeight * 0.5;
    for (const Platform& platform : platforms) {
        double platformLeft = platform.x - platform.width * 0.5;
        double platformRight = platform.x + platform.width * 0.5;
        double platformFront = platform.z - platform.depth * 0.5;
        double platformBack = platform.z + platform.depth * 0.5;
        double platformBottom = platform.y - 0.45;
        double platformTop = platform.y + 0.35;
        bool verticalOverlap = playerBottom < platformTop && playerTop > platformBottom;
        if (!verticalOverlap) continue;

        if (playerZ + PLAYER_RADIUS > platformFront && playerZ - PLAYER_RADIUS < platformBack) {
            if (previousX + PLAYER_RADIUS <= platformLeft && playerX + PLAYER_RADIUS > platformLeft) {
                playerX = platformLeft - PLAYER_RADIUS;
                velocityX = std::min(0.0, velocityX);
            } else if (previousX - PLAYER_RADIUS >= platformRight && playerX - PLAYER_RADIUS < platformRight) {
                playerX = platformRight + PLAYER_RADIUS;
                velocityX = std::max(0.0, velocityX);
            }
        }
        if (playerX + PLAYER_RADIUS > platformLeft && playerX - PLAYER_RADIUS < platformRight) {
            if (previousZ + PLAYER_RADIUS <= platformFront && playerZ + PLAYER_RADIUS > platformFront) {
                playerZ = platformFront - PLAYER_RADIUS;
                velocityZ = std::min(0.0, velocityZ);
            } else if (previousZ - PLAYER_RADIUS >= platformBack && playerZ - PLAYER_RADIUS < platformBack) {
                playerZ = platformBack + PLAYER_RADIUS;
                velocityZ = std::max(0.0, velocityZ);
            }
        }
    }

    grounded = false;
    double fallingSpeed = std::max(0.0, -verticalSpeed);
    verticalSpeed -= configuredGravity * deltaSeconds;
    double previousY = playerY;
    playerY += verticalSpeed * deltaSeconds;
    playerBottom = playerY - playerHeight * 0.5;
    playerTop = playerY + playerHeight * 0.5;
    for (const Platform& platform : platforms) {
        bool inside = std::abs(playerX - platform.x) < platform.width * 0.5 + PLAYER_RADIUS &&
                      std::abs(playerZ - platform.z) < platform.depth * 0.5 + PLAYER_RADIUS;
        double platformTop = platform.y + 0.35;
        double platformBottom = platform.y - 0.45;
        if (!inside) continue;
        if (verticalSpeed <= 0.0 && previousY - playerHeight * 0.5 >= platformTop && playerBottom <= platformTop) {
            playerY = platformTop + playerHeight * 0.5;
            verticalSpeed = 0.0;
            double impactForce = playerMass * fallingSpeed / IMPACT_TIME / 9.80665;
            if (impactForce > receptionCapacity) {
                fallen = true;
                fallElapsed = 0.0;
                rotationX = 1.0;
                velocityX = 0.0;
                velocityZ = 0.0;
            } else {
                grounded = true;
            }
        } else if (verticalSpeed > 0.0 && previousY + playerHeight * 0.5 <= platformBottom && playerTop >= platformBottom) {
            playerY = platformBottom - playerHeight * 0.5;
            verticalSpeed = 0.0;
        }
    }

    for (Platform& platform : platforms) {
        if (platform.floor) {
            platform.x = playerX;
            platform.z = playerZ + 60.0;
        }
    }

    if (keyState[VK_SPACE] && grounded) {
        verticalSpeed = jumpForce * 9.80665 / std::max(1.0, playerMass) * FORCE_APPLICATION_TIME;
        grounded = false;
    }
    if (playerY < -12.0) {
        playerX = 0.0;
        playerY = 0.35 + playerHeight * 0.5;
        playerZ = 0.0;
        velocityX = 0.0;
        velocityZ = 0.0;
        verticalSpeed = 0.0;
        grounded = true;
        fallen = false;
        fallElapsed = 0.0;
    }

    double movedFromMapCenter = std::hypot(playerX - lastMapCenterX, playerZ - lastMapCenterZ);
    if (movedFromMapCenter > 14.0) {
        lastMapCenterX = playerX;
        lastMapCenterZ = playerZ;
        int startIndex = static_cast<int>(platforms.size());
        int platformCount = std::max(8, mapRadius * 12 * platformDensity / 2);
        double spacing = 7.0 * platformSpacing / 100.0;
        double scale = platformSize / 100.0;
        for (int index = 0; index < platformCount; ++index) {
            double angle = 2.0 * PI * index / platformCount;
            double radius = (mapRadius * 2 + 1) * spacing;
            platforms.push_back({playerX + std::cos(angle) * radius,
                generatedPlatformHeight(startIndex + index),
                playerZ + std::sin(angle) * radius,
                PLATFORM_SIZE * scale, PLATFORM_SIZE * scale,
                (startIndex + index) % 3 == 0 ? RGB(255, 110, 90) : RGB(80, 210, 170), false});
        }
    }
    platforms.erase(std::remove_if(platforms.begin(), platforms.end(), [&](const Platform& platform) {
        return !platform.floor && std::hypot(platform.x - playerX, platform.z - playerZ) > (mapRadius * 3 + 4) * 7.0;
    }), platforms.end());
}

double cameraDepth(double x, double y, double z) {
    double dx = x - playerX;
    double dy = y - playerY;
    double dz = z - playerZ;
    double cameraYaw = rotationY + flipYaw;
    double cameraPitch = rotationX + flipPitch;
    double camZ = dx * std::sin(cameraYaw) + dz * std::cos(cameraYaw);
    return dy * std::sin(cameraPitch) + camZ * std::cos(cameraPitch);
}

struct CameraPoint {
    double x;
    double y;
    double depth;
};

CameraPoint toCameraPoint(double x, double y, double z) {
    double dx = x - playerX;
    double dy = y - playerY;
    double dz = z - playerZ;
    double cameraYaw = rotationY + flipYaw;
    double cameraPitch = rotationX + flipPitch;
    double camX = dx * std::cos(cameraYaw) - dz * std::sin(cameraYaw);
    double camZ = dx * std::sin(cameraYaw) + dz * std::cos(cameraYaw);
    double camY = dy * std::cos(cameraPitch) - camZ * std::sin(cameraPitch);
    return {camX, camY, dy * std::sin(cameraPitch) + camZ * std::cos(cameraPitch)};
}

void clipToNearPlane(std::vector<CameraPoint>& polygon) {
    constexpr double NEAR_PLANE = 0.2;
    if (polygon.empty()) return;
    std::vector<CameraPoint> clipped;
    CameraPoint previous = polygon.back();
    bool previousInside = previous.depth >= NEAR_PLANE;
    for (const CameraPoint& current : polygon) {
        bool currentInside = current.depth >= NEAR_PLANE;
        if (currentInside != previousInside) {
            double denominator = current.depth - previous.depth;
            double ratio = denominator == 0.0 ? 0.0 : (NEAR_PLANE - previous.depth) / denominator;
            clipped.push_back({
                previous.x + (current.x - previous.x) * ratio,
                previous.y + (current.y - previous.y) * ratio,
                NEAR_PLANE
            });
        }
        if (currentInside) clipped.push_back(current);
        previous = current;
        previousInside = currentInside;
    }
    polygon = std::move(clipped);
}

POINT projectCameraPoint(const CameraPoint& cameraPoint, int width, int height, int centerShift) {
    double rolledX = cameraPoint.x * std::cos(flipRoll) - cameraPoint.y * std::sin(flipRoll);
    double rolledY = cameraPoint.x * std::sin(flipRoll) + cameraPoint.y * std::cos(flipRoll);
    double focal = std::min(width, height) * 0.5 / std::tan(fieldOfView * 0.5 * PI / 180.0);
    return {
        static_cast<LONG>(width / 2.0 + centerShift + rolledX * focal / cameraPoint.depth),
        static_cast<LONG>(height * 0.52 - rolledY * focal / cameraPoint.depth)
    };
}

void drawScene(HDC deviceContext, int panelLeft, int panelWidth, int height, int centerShift) {
    int savedState = SaveDC(deviceContext);
    IntersectClipRect(deviceContext, panelLeft, 0, panelLeft + panelWidth, height);

    RECT clientRect{panelLeft, 0, panelLeft + panelWidth, height};
    HBRUSH background = CreateSolidBrush(RGB(12, 15, 25));
    FillRect(deviceContext, &clientRect, background);
    DeleteObject(background);

    for (int index = 0; index < starCount; ++index) {
        int starX = (index * 7919 + panelWidth * 17) % std::max(1, panelWidth);
        int starY = (index * 1543 + height * 11) % std::max(1, height);
        SetPixel(deviceContext, panelLeft + starX, starY,
            pointLight ? RGB(180, 210, 255) : RGB(90, 105, 125));
    }

    std::vector<std::pair<double, size_t>> visible;
    for (size_t index = 0; index < platforms.size(); ++index) {
        const Platform& platform = platforms[index];
        visible.push_back({cameraDepth(platform.x, platform.y, platform.z), index});
    }
    // Painter's algorithm: farthest objects first, nearest objects last.
    std::sort(visible.rbegin(), visible.rend());

    for (const auto& item : visible) {
        const Platform& platform = platforms[item.second];
        if (platform.floor) continue;
        std::vector<CameraPoint> top = {
            toCameraPoint(platform.x - platform.width / 2, platform.y, platform.z - platform.depth / 2),
            toCameraPoint(platform.x + platform.width / 2, platform.y, platform.z - platform.depth / 2),
            toCameraPoint(platform.x + platform.width / 2, platform.y, platform.z + platform.depth / 2),
            toCameraPoint(platform.x - platform.width / 2, platform.y, platform.z + platform.depth / 2)
        };
        std::vector<CameraPoint> bottom = {
            toCameraPoint(platform.x - platform.width / 2, platform.y - 0.45, platform.z - platform.depth / 2),
            toCameraPoint(platform.x + platform.width / 2, platform.y - 0.45, platform.z - platform.depth / 2),
            toCameraPoint(platform.x + platform.width / 2, platform.y - 0.45, platform.z + platform.depth / 2),
            toCameraPoint(platform.x - platform.width / 2, platform.y - 0.45, platform.z + platform.depth / 2)
        };
        COLORREF sideColor = RGB(
            GetRValue(platform.color) * 2 / 3,
            GetGValue(platform.color) * 2 / 3,
            GetBValue(platform.color) * 2 / 3);
        if (!pointLight) {
            sideColor = RGB(GetRValue(sideColor) / 2, GetGValue(sideColor) / 2, GetBValue(sideColor) / 2);
        }
        auto drawFace = [&](std::vector<CameraPoint> face, COLORREF color) {
            clipToNearPlane(face);
            if (face.size() < 3) return;
            std::vector<POINT> projected;
            projected.reserve(face.size());
            for (const CameraPoint& point : face) {
                POINT screenPoint = projectCameraPoint(point, panelWidth, height, centerShift);
                screenPoint.x += panelLeft;
                projected.push_back(screenPoint);
            }
            HBRUSH brush = CreateSolidBrush(color);
            HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(deviceContext, brush));
            Polygon(deviceContext, projected.data(), static_cast<int>(projected.size()));
            SelectObject(deviceContext, oldBrush);
            DeleteObject(brush);
        };
        drawFace({top[0], top[1], bottom[1], bottom[0]}, sideColor);
        drawFace({top[1], top[2], bottom[2], bottom[1]}, sideColor);
        drawFace({top[2], top[3], bottom[3], bottom[2]}, sideColor);
        drawFace({top[3], top[0], bottom[0], bottom[3]}, sideColor);
        drawFace(top, platform.color);
    }

    RestoreDC(deviceContext, savedState);
}

HWND makeMenuButton(HWND parent, const wchar_t* text, int id) {
    return CreateWindow(L"BUTTON", text, WS_CHILD | BS_PUSHBUTTON,
        0, 0, 120, 32, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandle(nullptr), nullptr);
}

struct SliderSpec {
    int maximumPosition;
};

SliderSpec sliderSpecFor(int optionId) {
    if (optionId == 3101) return {5};
    if (optionId == 3111) return {7};
    if (optionId == 3121 || optionId == 3131 || optionId == 3141) return {15};
    if (optionId == 3151) return {20};
    if (optionId == 3201) return {18};
    if (optionId == 3231) return {7};
    if (optionId == 3301) return {19};
    if (optionId == 3321) return {16};
    if (optionId == 3501) return {39};
    if (optionId == 3511) return {195};
    if (optionId == 3521) return {400};
    if (optionId == 3531) return {895};
    if (optionId == 3541) return {400};
    if (optionId == 3551) return {199};
    return {28};
}

double menuOptionValue(int optionId) {
    if (optionId == 3101) return mapRadius;
    if (optionId == 3111) return platformDensity;
    if (optionId == 3121) return platformSize;
    if (optionId == 3131) return platformHeight;
    if (optionId == 3141) return platformSpacing;
    if (optionId == 3151) return starCount;
    if (optionId == 3201) return fieldOfView;
    if (optionId == 3231) return fpsLimit;
    if (optionId == 3301) return configuredMoveSpeed;
    if (optionId == 3321) return playerHeight;
    if (optionId == 3501) return configuredAcceleration;
    if (optionId == 3511) return playerMass;
    if (optionId == 3521) return jumpForce;
    if (optionId == 3531) return receptionCapacity;
    if (optionId == 3541) return configuredGravity;
    if (optionId == 3551) return stopAcceleration;
    if (optionId == 3421) return mortalSpeed;
    return twistSpeed;
}

int sliderPositionFor(int optionId) {
    double value = menuOptionValue(optionId);
    if (optionId == 3101) return static_cast<int>(value - 1);
    if (optionId == 3111) return static_cast<int>(value - 1);
    if (optionId == 3121 || optionId == 3131 || optionId == 3141) return static_cast<int>((value - 50.0) / 10.0);
    if (optionId == 3151) return static_cast<int>(value / 50.0);
    if (optionId == 3201) return static_cast<int>((value - 50.0) / 5.0);
    if (optionId == 3231) return static_cast<int>((value - 30.0) / 30.0);
    if (optionId == 3301) return static_cast<int>(value - 1);
    if (optionId == 3321) return static_cast<int>((value - 1.40) / 0.05);
    if (optionId == 3501) return static_cast<int>(value - 1.0);
    if (optionId == 3511) return static_cast<int>(value - 5.0);
    if (optionId == 3521) return static_cast<int>(value);
    if (optionId == 3531) return static_cast<int>(value - 5.0);
    if (optionId == 3541) return static_cast<int>(value * 10.0);
    if (optionId == 3551) return static_cast<int>((value - 0.1) * 10.0);
    return static_cast<int>((value - 0.1) / 0.05);
}

double sliderValueFor(int optionId, int position) {
    if (optionId == 3101 || optionId == 3111) return position + 1.0;
    if (optionId == 3121 || optionId == 3131 || optionId == 3141) return 50.0 + position * 10.0;
    if (optionId == 3151) return position * 50.0;
    if (optionId == 3201) return 50.0 + position * 5.0;
    if (optionId == 3231) return 30.0 + position * 30.0;
    if (optionId == 3301) return position + 1.0;
    if (optionId == 3321) return 1.40 + position * 0.05;
    if (optionId == 3501) return 1.0 + position;
    if (optionId == 3511) return 5.0 + position;
    if (optionId == 3521) return position;
    if (optionId == 3531) return 5.0 + position;
    if (optionId == 3541) return position * 0.1;
    if (optionId == 3551) return 0.1 + position * 0.1;
    return 0.1 + position * 0.05;
}

void addMenuOption(HWND parent, int page, const wchar_t* title, int minusId, int plusId) {
    HWND label = CreateWindow(L"STATIC", title, WS_CHILD | SS_CENTER,
        0, 0, 180, 30, parent, nullptr, GetModuleHandle(nullptr), nullptr);
    HWND slider = CreateWindowEx(
        0,
        TRACKBAR_CLASS,
        title,
        WS_CHILD | TBS_AUTOTICKS,
        0, 0, 180, 34,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(minusId)),
        GetModuleHandle(nullptr),
        nullptr);
    SliderSpec spec = sliderSpecFor(minusId);
    SendMessage(slider, TBM_SETRANGE, TRUE, MAKELONG(0, spec.maximumPosition));
    SendMessage(slider, TBM_SETLINESIZE, TRUE, 1);
    SendMessage(slider, TBM_SETPOS, TRUE, sliderPositionFor(minusId));
    menuOptions.push_back({page, minusId, plusId, label, slider});
}

void updateMenuLabels() {
    for (const MenuOption& option : menuOptions) {
        wchar_t text[64]{};
        int value = 0;
        const wchar_t* name = L"";
        if (option.page == 1) {
            if (option.minusId == 3101) { name = L"Radius"; value = mapRadius; }
            if (option.minusId == 3111) { name = L"Density"; value = platformDensity; }
            if (option.minusId == 3121) { name = L"Size"; value = platformSize; }
            if (option.minusId == 3131) { name = L"Height"; value = platformHeight; }
            if (option.minusId == 3141) { name = L"Spacing"; value = platformSpacing; }
            if (option.minusId == 3151) { name = L"Stars"; value = starCount; }
        } else if (option.page == 2) {
            if (option.minusId == 3201) { name = L"FOV"; value = fieldOfView; }
            if (option.minusId == 3231) { name = L"FPS limit"; value = fpsLimit; }
        } else if (option.page == 3) {
            if (option.minusId == 3301) { name = L"Speed m/s"; value = configuredMoveSpeed; }
            if (option.minusId == 3321) {
                swprintf_s(text, L"Height: %.2f m", playerHeight);
                SetWindowText(option.label, text);
                continue;
            }
        } else if (option.page == 4) {
            if (option.minusId == 3421) {
                swprintf_s(text, L"Mortal speed: %.2f", mortalSpeed);
                SetWindowText(option.label, text);
                continue;
            }
            if (option.minusId == 3431) {
                swprintf_s(text, L"Twist speed: %.2f", twistSpeed);
                SetWindowText(option.label, text);
                continue;
            }
        } else if (option.page == 5) {
            if (option.minusId == 3501) {
                swprintf_s(text, L"Acceleration: %.1f m/s2", configuredAcceleration);
                SetWindowText(option.label, text);
                continue;
            }
            if (option.minusId == 3511) {
                swprintf_s(text, L"Mass: %.0f kg", playerMass);
                SetWindowText(option.label, text);
                continue;
            }
            if (option.minusId == 3521) {
                swprintf_s(text, L"Jump force: %.0f kgf", jumpForce);
                SetWindowText(option.label, text);
                continue;
            }
            if (option.minusId == 3531) {
                swprintf_s(text, L"Landing capacity: %.0f kgf", receptionCapacity);
                SetWindowText(option.label, text);
                continue;
            }
            if (option.minusId == 3541) {
                swprintf_s(text, L"Gravity: %.1f m/s2", configuredGravity);
                SetWindowText(option.label, text);
                continue;
            }
            if (option.minusId == 3551) {
                swprintf_s(text, L"Stop acceleration: %.1f m/s2", stopAcceleration);
                SetWindowText(option.label, text);
                continue;
            }
        }
        swprintf_s(text, L"%s: %d", name, value);
        SetWindowText(option.label, text);
    }
}

void layoutMenuControls(int width, int height);

void showMenuPage(int page) {
    menuPage = page;
    ShowWindow(resumeButton, page == 0 && paused ? SW_SHOW : SW_HIDE);
    ShowWindow(restartButton, page == 0 && paused ? SW_SHOW : SW_HIDE);
    ShowWindow(exitButton, page == 0 && paused ? SW_SHOW : SW_HIDE);
    ShowWindow(mapButton, page == 0 && paused ? SW_SHOW : SW_HIDE);
    ShowWindow(graphicsButton, page == 0 && paused ? SW_SHOW : SW_HIDE);
    ShowWindow(playerButton, page == 0 && paused ? SW_SHOW : SW_HIDE);
    ShowWindow(flipsButton, page == 0 && paused ? SW_SHOW : SW_HIDE);
    ShowWindow(physicsButton, page == 0 && paused ? SW_SHOW : SW_HIDE);
    ShowWindow(backButton, page != 0 && paused ? SW_SHOW : SW_HIDE);
    ShowWindow(pointLightButton, page == 2 && paused ? SW_SHOW : SW_HIDE);
    ShowWindow(showFpsButton, page == 2 && paused ? SW_SHOW : SW_HIDE);
    for (const MenuOption& option : menuOptions) {
        bool visible = paused && page == option.page;
        ShowWindow(option.label, visible ? SW_SHOW : SW_HIDE);
        ShowWindow(option.slider, visible ? SW_SHOW : SW_HIDE);
    }
    updateMenuLabels();
    if (gameWindow) InvalidateRect(gameWindow, nullptr, FALSE);
    if (resumeButton) {
        RECT clientRect{};
        GetClientRect(GetParent(resumeButton), &clientRect);
        layoutMenuControls(clientRect.right, clientRect.bottom);
        InvalidateRect(GetParent(resumeButton), nullptr, FALSE);
    }
}

void adjustMenuOption(int commandId) {
    int direction = commandId % 10 == 1 ? -1 : 1;
    if (commandId == 3101 || commandId == 3102) mapRadius = std::clamp(mapRadius + direction, 1, 6);
    if (commandId == 3111 || commandId == 3112) platformDensity = std::clamp(platformDensity + direction, 1, 8);
    if (commandId == 3121 || commandId == 3122) platformSize = std::clamp(platformSize + direction * 10, 50, 200);
    if (commandId == 3131 || commandId == 3132) platformHeight = std::clamp(platformHeight + direction * 10, 50, 200);
    if (commandId == 3141 || commandId == 3142) platformSpacing = std::clamp(platformSpacing + direction * 10, 50, 200);
    if (commandId == 3151 || commandId == 3152) starCount = std::clamp(starCount + direction * 50, 0, 1000);
    if (commandId == 3201 || commandId == 3202) fieldOfView = std::clamp(fieldOfView + direction * 5, 50, 140);
    if (commandId == 3231 || commandId == 3232) fpsLimit = std::clamp(fpsLimit + direction * 30, 30, 240);
    if (commandId == 3301 || commandId == 3302) configuredMoveSpeed = std::clamp(configuredMoveSpeed + direction, 1, 20);
    if (commandId == 3321 || commandId == 3322) {
        playerHeight = std::clamp(playerHeight + direction * 0.05, 1.40, 2.20);
            if (gameWindow) {
            playerY = 0.35 + playerHeight * 0.5;
            verticalSpeed = 0.0;
        }
    }
    if (commandId == 3421 || commandId == 3422) {
        mortalSpeed = std::clamp(mortalSpeed + direction * 0.05, 0.1, 1.5);
            if (activeFlipMask != 0) flipDuration = 1.0 / mortalSpeed;
    }
    if (commandId == 3431 || commandId == 3432) {
        twistSpeed = std::clamp(twistSpeed + direction * 0.05, 0.1, 1.5);
            if (activeFlipMask != 0) flipDuration = 1.0 / twistSpeed;
    }
    if (commandId >= 3101 && commandId <= 3152) resetGame();
    updateMenuLabels();
    if ((commandId == 3231 || commandId == 3232) && gameWindow) {
        SetTimer(gameWindow, 1, std::max(1, static_cast<int>(std::ceil(1000.0 / fpsLimit))), nullptr);
    }
    if (gameWindow) InvalidateRect(gameWindow, nullptr, FALSE);
}

void setMenuOptionFromSlider(int commandId, int position) {
    double value = sliderValueFor(commandId, position);
    if (commandId == 3101) mapRadius = static_cast<int>(value);
    if (commandId == 3111) platformDensity = static_cast<int>(value);
    if (commandId == 3121) platformSize = static_cast<int>(value);
    if (commandId == 3131) platformHeight = static_cast<int>(value);
    if (commandId == 3141) platformSpacing = static_cast<int>(value);
    if (commandId == 3151) starCount = static_cast<int>(value);
    if (commandId == 3201) fieldOfView = static_cast<int>(value);
    if (commandId == 3231) fpsLimit = static_cast<int>(value);
    if (commandId == 3301) configuredMoveSpeed = static_cast<int>(value);
    if (commandId == 3321) {
        playerHeight = value;
        playerY = 0.35 + playerHeight * 0.5;
        verticalSpeed = 0.0;
    }
    if (commandId == 3421) mortalSpeed = value;
    if (commandId == 3431) twistSpeed = value;
    if (commandId == 3501) configuredAcceleration = value;
    if (commandId == 3511) playerMass = value;
    if (commandId == 3521) jumpForce = value;
    if (commandId == 3531) receptionCapacity = value;
    if (commandId == 3541) configuredGravity = value;
    if (commandId == 3551) stopAcceleration = value;
    if (commandId == 3421 || commandId == 3431) {
        if (activeFlipMask != 0) flipDuration = 1.0 / value;
    }
    if (commandId >= 3101 && commandId <= 3151) resetGame();
    if (commandId == 3231 && gameWindow) {
        SetTimer(gameWindow, 1, std::max(1, static_cast<int>(std::ceil(1000.0 / fpsLimit))), nullptr);
    }
    updateMenuLabels();
    if (gameWindow) InvalidateRect(gameWindow, nullptr, FALSE);
}

int menuHeightForPage(int page) {
    if (page == 1) return 500;
    if (page == 2) return 230;
    if (page == 3) return 230;
    if (page == 4) return 230;
    if (page == 5) return 500;
    return 170;
}

int menuTopForPage(int page, int renderHeight) {
    return std::max(36, (renderHeight - menuHeightForPage(page)) / 2);
}

void layoutMenuControls(int width, int height) {
    int renderHeight = std::max(1, height - CONTROL_HEIGHT);
    int menuTop = menuTopForPage(menuPage, renderHeight);
    int center = width / 2;
    int rootTop = menuTop + 38;

    MoveWindow(resumeButton, center - 150, rootTop, 90, 34, TRUE);
    MoveWindow(restartButton, center - 45, rootTop, 90, 34, TRUE);
    MoveWindow(exitButton, center + 60, rootTop, 90, 34, TRUE);
    MoveWindow(flipsButton, center + 46, rootTop + 48, 82, 32, TRUE);
    MoveWindow(physicsButton, center + 134, rootTop + 48, 82, 32, TRUE);
    MoveWindow(mapButton, center - 216, rootTop + 48, 82, 32, TRUE);
    MoveWindow(graphicsButton, center - 128, rootTop + 48, 82, 32, TRUE);
    MoveWindow(playerButton, center - 40, rootTop + 48, 82, 32, TRUE);
    MoveWindow(backButton, center - 45, menuTop + 8, 90, 32, TRUE);
    MoveWindow(pointLightButton, center - 160, menuTop + 48, 120, 32, TRUE);
    MoveWindow(showFpsButton, center + 40, menuTop + 48, 120, 32, TRUE);

    int optionIndex = 0;
    int optionTop = menuTop + 48;
    for (const MenuOption& option : menuOptions) {
        if (option.page != menuPage) continue;
        int y = optionTop + optionIndex++ * 72;
        MoveWindow(option.label, center - 90, y, 180, 30, TRUE);
        MoveWindow(option.slider, center - 110, y + 30, 220, 34, TRUE);
    }
}

void drawPausePanel(HDC deviceContext, int panelLeft, int panelWidth, int renderHeight) {
    int menuHeight = menuHeightForPage(menuPage);
    int menuTop = menuTopForPage(menuPage, renderHeight);
    int center = panelLeft + panelWidth / 2;
    RECT pausePanel{center - 220, menuTop, center + 220, menuTop + menuHeight};
    HBRUSH pauseBrush = CreateSolidBrush(RGB(20, 24, 38));
    FillRect(deviceContext, &pausePanel, pauseBrush);
    DeleteObject(pauseBrush);
    SetBkMode(deviceContext, TRANSPARENT);
    SetTextColor(deviceContext, RGB(255, 255, 255));
    const wchar_t* title = menuPage == 0 ? L"PAUSED" :
        (menuPage == 1 ? L"MAPA" : menuPage == 2 ? L"GRAFICOS" :
        menuPage == 3 ? L"JUGADOR" : menuPage == 4 ? L"MORTALES" : L"FISICA");
    RECT titleRect{pausePanel.left, pausePanel.top + 8, pausePanel.right, pausePanel.top + 36};
    DrawText(deviceContext, title, -1, &titleRect, DT_CENTER | DT_TOP);
}

LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        INITCOMMONCONTROLSEX commonControls{};
        commonControls.dwSize = sizeof(commonControls);
        commonControls.dwICC = ICC_BAR_CLASSES;
        InitCommonControlsEx(&commonControls);

        eyeDistanceLabel = CreateWindow(
            L"STATIC",
            L"Distancia ocular",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            0,
            0,
            180,
            24,
            window,
            nullptr,
            GetModuleHandle(nullptr),
            nullptr
        );
        eyeDistanceSlider = CreateWindowEx(
            0,
            TRACKBAR_CLASS,
            L"Distancia ocular",
            WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
            0,
            0,
            300,
            40,
            window,
            reinterpret_cast<HMENU>(1001),
            GetModuleHandle(nullptr),
            nullptr
        );
        SendMessage(eyeDistanceSlider, TBM_SETRANGE, TRUE, MAKELONG(MIN_EYE_DISTANCE, MAX_EYE_DISTANCE));
        SendMessage(eyeDistanceSlider, TBM_SETPOS, TRUE, DEFAULT_EYE_DISTANCE);
        resumeButton = CreateWindow(L"BUTTON", L"Resume", WS_CHILD | BS_PUSHBUTTON,
            0, 0, 130, 34, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(BUTTON_RESUME)), GetModuleHandle(nullptr), nullptr);
        restartButton = CreateWindow(L"BUTTON", L"Restart", WS_CHILD | BS_PUSHBUTTON,
            0, 0, 130, 34, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(BUTTON_RESTART)), GetModuleHandle(nullptr), nullptr);
        exitButton = CreateWindow(L"BUTTON", L"Exit", WS_CHILD | BS_PUSHBUTTON,
            0, 0, 130, 34, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(BUTTON_EXIT)), GetModuleHandle(nullptr), nullptr);
        mapButton = makeMenuButton(window, L"Mapa", BUTTON_MAP);
        graphicsButton = makeMenuButton(window, L"Graficos", BUTTON_GRAPHICS);
        playerButton = makeMenuButton(window, L"Jugador", BUTTON_PLAYER);
        flipsButton = makeMenuButton(window, L"Mortales", BUTTON_FLIPS);
        physicsButton = makeMenuButton(window, L"Fisica", BUTTON_PHYSICS);
        backButton = makeMenuButton(window, L"Back", BUTTON_BACK);
        pointLightButton = makeMenuButton(window, L"Point Light", BUTTON_POINT_LIGHT);
        showFpsButton = makeMenuButton(window, L"Show FPS", BUTTON_SHOW_FPS);

        addMenuOption(window, 1, L"Radius", 3101, 3102);
        addMenuOption(window, 1, L"Density", 3111, 3112);
        addMenuOption(window, 1, L"Size", 3121, 3122);
        addMenuOption(window, 1, L"Height", 3131, 3132);
        addMenuOption(window, 1, L"Spacing", 3141, 3142);
        addMenuOption(window, 1, L"Stars", 3151, 3152);
        addMenuOption(window, 2, L"FOV", 3201, 3202);
        addMenuOption(window, 2, L"FPS limit", 3231, 3232);
        addMenuOption(window, 3, L"Speed m/s", 3301, 3302);
        addMenuOption(window, 3, L"Height", 3321, 3322);
        addMenuOption(window, 4, L"Mortal speed", 3421, 3422);
        addMenuOption(window, 4, L"Twist speed", 3431, 3432);
        addMenuOption(window, 5, L"Acceleration", 3501, 3502);
        addMenuOption(window, 5, L"Mass", 3511, 3512);
        addMenuOption(window, 5, L"Jump force", 3521, 3522);
        addMenuOption(window, 5, L"Landing capacity", 3531, 3532);
        addMenuOption(window, 5, L"Gravity", 3541, 3542);
        addMenuOption(window, 5, L"Stop acceleration", 3551, 3552);
        showMenuPage(0);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC deviceContext = BeginPaint(window, &paint);
        RECT clientRect{};
        GetClientRect(window, &clientRect);

        HDC backBuffer = CreateCompatibleDC(deviceContext);
        HBITMAP backBitmap = CreateCompatibleBitmap(deviceContext, clientRect.right, clientRect.bottom);
        HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(backBuffer, backBitmap));

        int renderHeight = std::max(1, static_cast<int>(clientRect.bottom) - CONTROL_HEIGHT);
        int panelWidth = clientRect.right / 2;
        int leftShift = clientRect.right / 4 - eyeDistance / 2;
        drawScene(backBuffer, 0, panelWidth, renderHeight, leftShift);
        drawScene(backBuffer, panelWidth, clientRect.right - panelWidth, renderHeight, -leftShift);

        RECT divider{panelWidth - 1, 0, panelWidth + 1, renderHeight};
        HBRUSH dividerBrush = CreateSolidBrush(RGB(70, 80, 100));
        FillRect(backBuffer, &divider, dividerBrush);
        DeleteObject(dividerBrush);

        RECT controlBackground{0, renderHeight, clientRect.right, clientRect.bottom};
        HBRUSH controlBrush = CreateSolidBrush(RGB(24, 28, 42));
        FillRect(backBuffer, &controlBackground, controlBrush);
        DeleteObject(controlBrush);

        if (paused) {
            drawPausePanel(backBuffer, 0, panelWidth, renderHeight);
            drawPausePanel(backBuffer, panelWidth, clientRect.right - panelWidth, renderHeight);
        }
        if (showFps) {
            wchar_t fpsText[32]{};
            swprintf_s(fpsText, L"FPS: %d", displayedFps);
            RECT fpsRect{12, 12, 130, 38};
            SetBkMode(backBuffer, TRANSPARENT);
            SetTextColor(backBuffer, RGB(255, 255, 255));
            DrawText(backBuffer, fpsText, -1, &fpsRect, DT_LEFT | DT_TOP);
        }

        BitBlt(deviceContext, 0, 0, clientRect.right, clientRect.bottom,
            backBuffer, 0, 0, SRCCOPY);
        SelectObject(backBuffer, oldBitmap);
        DeleteObject(backBitmap);
        DeleteDC(backBuffer);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_CTLCOLORSTATIC: {
        HDC staticContext = reinterpret_cast<HDC>(wParam);
        SetBkMode(staticContext, TRANSPARENT);
        SetTextColor(staticContext, RGB(240, 245, 255));
        return reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH));
    }
    case WM_KEYDOWN:
        if (!paused && !keyState[wParam < 256 ? wParam : 0]) {
            if (wParam == VK_NUMPAD8) startFlip(FlipKind::Front);
            if (wParam == VK_NUMPAD2) startFlip(FlipKind::Back);
            if (wParam == VK_NUMPAD4) startFlip(FlipKind::TwistLeft);
            if (wParam == VK_NUMPAD6) startFlip(FlipKind::TwistRight);
            if (wParam == VK_NUMPAD1) startFlip(FlipKind::SideLeft);
            if (wParam == VK_NUMPAD3) startFlip(FlipKind::SideRight);
        }
        if (wParam < 256) keyState[wParam] = true;
        if (wParam == 'P') {
            paused = !paused;
            showMenuPage(0);
            InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    case WM_KEYUP:
        if (wParam < 256) keyState[wParam] = false;
        return 0;
    case WM_KILLFOCUS:
    case WM_ACTIVATEAPP:
        if (message == WM_KILLFOCUS || !wParam) {
            std::fill(std::begin(keyState), std::end(keyState), false);
            dragging = false;
            if (GetCapture() == window) ReleaseCapture();
        }
        return 0;
    case WM_TIMER: {
        ULONGLONG now = GetTickCount64();
        double deltaSeconds = lastTick == 0 ? 0.016 : std::min(0.05, (now - lastTick) / 1000.0);
        lastTick = now;
        updateGame(deltaSeconds);
        ++frameCounter;
        if (fpsWindowStart == 0) fpsWindowStart = now;
        if (now - fpsWindowStart >= 1000) {
            displayedFps = frameCounter;
            frameCounter = 0;
            fpsWindowStart = now;
        }
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    }
    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(lParam) == eyeDistanceSlider) {
            eyeDistance = static_cast<int>(SendMessage(eyeDistanceSlider, TBM_GETPOS, 0, 0));
            InvalidateRect(window, nullptr, FALSE);
            SetFocus(window);
        }
        if (reinterpret_cast<HWND>(lParam) != eyeDistanceSlider) {
            HWND slider = reinterpret_cast<HWND>(lParam);
            int commandId = GetDlgCtrlID(slider);
            for (const MenuOption& option : menuOptions) {
                if (option.slider == slider) {
                    setMenuOptionFromSlider(commandId, static_cast<int>(SendMessage(slider, TBM_GETPOS, 0, 0)));
                    break;
                }
            }
            if (LOWORD(wParam) == TB_ENDTRACK) SetFocus(window);
        }
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case BUTTON_RESUME:
            paused = false;
            showMenuPage(0);
            SetFocus(window);
            return 0;
        case BUTTON_RESTART:
            resetGame();
            paused = false;
            showMenuPage(0);
            SetFocus(window);
            return 0;
        case BUTTON_EXIT:
            DestroyWindow(window);
            return 0;
        case BUTTON_MAP:
            showMenuPage(1);
            return 0;
        case BUTTON_GRAPHICS:
            showMenuPage(2);
            return 0;
        case BUTTON_PLAYER:
            showMenuPage(3);
            return 0;
        case BUTTON_FLIPS:
            showMenuPage(4);
            return 0;
        case BUTTON_PHYSICS:
            showMenuPage(5);
            return 0;
        case BUTTON_BACK:
            showMenuPage(0);
            return 0;
        case BUTTON_POINT_LIGHT:
            pointLight = !pointLight;
            SetWindowText(pointLightButton, pointLight ? L"Point Light: ON" : L"Point Light: OFF");
            return 0;
        case BUTTON_SHOW_FPS:
            showFps = !showFps;
            SetWindowText(showFpsButton, showFps ? L"Show FPS: ON" : L"Show FPS: OFF");
            return 0;
        default:
            if (LOWORD(wParam) >= 3101 && LOWORD(wParam) <= 3432) {
                adjustMenuOption(LOWORD(wParam));
            }
            break;
        }
        return 0;
    case WM_LBUTTONDOWN: {
        lastMouse = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        dragging = true;
        SetCapture(window);
        return 0;
    }
    case WM_LBUTTONUP: {
        POINT currentMouse{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        (void)currentMouse;
        dragging = false;
        ReleaseCapture();
        return 0;
    }
    case WM_MOUSEMOVE:
        if (dragging) {
            POINT currentMouse{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            rotationY += (currentMouse.x - lastMouse.x) * 0.01;
            rotationX = std::clamp(rotationX + (currentMouse.y - lastMouse.y) * 0.01, -1.0, 1.0);
            lastMouse = currentMouse;
            InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    case WM_SIZE:
        if (eyeDistanceSlider && eyeDistanceLabel) {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            int sliderWidth = std::max(220, static_cast<int>(width) - 260);
            int sliderLeft = (width - sliderWidth) / 2;
            MoveWindow(eyeDistanceLabel, sliderLeft, height - 72, sliderWidth, 24, TRUE);
            MoveWindow(eyeDistanceSlider, sliderLeft, height - 48, sliderWidth, 40, TRUE);
            layoutMenuControls(width, height);
        }
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(window, message, wParam, lParam);
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    resetGame();

    const wchar_t className[] = L"ClickableSphereWindow";
    WNDCLASS windowClass{};
    windowClass.hInstance = instance;
    windowClass.lpfnWndProc = windowProcedure;
    windowClass.lpszClassName = className;
    windowClass.hCursor = LoadCursor(nullptr, IDC_HAND);
    windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    RegisterClass(&windowClass);

    HWND window = CreateWindowEx(
        WS_EX_COMPOSITED,
        className,
        L"Esfera de motas clickable",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        900,
        700,
        nullptr,
        nullptr,
        instance,
        nullptr
    );

    if (!window) {
        return 1;
    }

    gameWindow = window;
    ShowWindow(window, showCommand);
    UpdateWindow(window);
    ShowWindow(resumeButton, SW_HIDE);
    ShowWindow(restartButton, SW_HIDE);
    ShowWindow(exitButton, SW_HIDE);
    showMenuPage(0);
    SetTimer(window, 1, std::max(1, static_cast<int>(std::ceil(1000.0 / fpsLimit))), nullptr);
    SetFocus(window);

    MSG message{};
    while (GetMessage(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
    return static_cast<int>(message.wParam);
}
