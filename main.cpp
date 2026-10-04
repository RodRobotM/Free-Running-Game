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
constexpr double MOVE_SPEED = 7.0;
constexpr double JUMP_SPEED = 8.5;
constexpr double GRAVITY = 22.0;
constexpr double PLATFORM_SIZE = 4.0;
constexpr int BUTTON_RESUME = 2001;
constexpr int BUTTON_RESTART = 2002;
constexpr int BUTTON_EXIT = 2003;
constexpr int BUTTON_MAP = 3001;
constexpr int BUTTON_GRAPHICS = 3002;
constexpr int BUTTON_PLAYER = 3003;
constexpr int BUTTON_FLIPS = 3004;
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
double playerX = 0.0;
double playerY = 4.0;
double playerZ = 0.0;
double playerHeight = 1.74;
double verticalSpeed = 0.0;
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
int configuredJump = 9;
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
HWND backButton = nullptr;
HWND pointLightButton = nullptr;
HWND showFpsButton = nullptr;
HWND gameWindow = nullptr;

struct MenuOption {
    int page;
    int minusId;
    int plusId;
    HWND label;
    HWND minusButton;
    HWND plusButton;
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
    verticalSpeed = 0.0;
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
        verticalSpeed = std::max(verticalSpeed, JUMP_SPEED * 0.7);
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

    double previousX = playerX;
    double previousZ = playerZ;
    double forward = (keyState[VK_UP] ? 1.0 : 0.0) - (keyState[VK_DOWN] ? 1.0 : 0.0);
    double strafe = (keyState[VK_RIGHT] ? 1.0 : 0.0) - (keyState[VK_LEFT] ? 1.0 : 0.0);
    double length = std::sqrt(forward * forward + strafe * strafe);
    if (length > 0.0) {
        forward /= length;
        strafe /= length;
        playerX += (std::sin(rotationY) * forward + std::cos(rotationY) * strafe) * configuredMoveSpeed * deltaSeconds;
        playerZ += (std::cos(rotationY) * forward - std::sin(rotationY) * strafe) * configuredMoveSpeed * deltaSeconds;
    }

    if (keyState['A']) rotationY -= 1.8 * deltaSeconds;
    if (keyState['D']) rotationY += 1.8 * deltaSeconds;
    if (keyState['W']) rotationX = std::max(-1.0, rotationX - 1.4 * deltaSeconds);
    if (keyState['S']) rotationX = std::min(1.0, rotationX + 1.4 * deltaSeconds);
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
            } else if (previousX - PLAYER_RADIUS >= platformRight && playerX - PLAYER_RADIUS < platformRight) {
                playerX = platformRight + PLAYER_RADIUS;
            }
        }
        if (playerX + PLAYER_RADIUS > platformLeft && playerX - PLAYER_RADIUS < platformRight) {
            if (previousZ + PLAYER_RADIUS <= platformFront && playerZ + PLAYER_RADIUS > platformFront) {
                playerZ = platformFront - PLAYER_RADIUS;
            } else if (previousZ - PLAYER_RADIUS >= platformBack && playerZ - PLAYER_RADIUS < platformBack) {
                playerZ = platformBack + PLAYER_RADIUS;
            }
        }
    }

    verticalSpeed -= GRAVITY * deltaSeconds;
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

    if (keyState[VK_SPACE] && std::abs(verticalSpeed) < 0.05) {
        verticalSpeed = configuredJump;
    }
    if (playerY < -12.0) {
        playerX = 0.0;
        playerY = 0.35 + playerHeight * 0.5;
        playerZ = 0.0;
        verticalSpeed = 0.0;
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

bool projectPoint(double x, double y, double z, int width, int height, int centerShift, POINT& point, double& depth) {
    double dx = x - playerX;
    double dy = y - playerY;
    double dz = z - playerZ;
    double cameraYaw = rotationY + flipYaw;
    double cameraPitch = rotationX + flipPitch;
    double camX = dx * std::cos(cameraYaw) - dz * std::sin(cameraYaw);
    double camZ = dx * std::sin(cameraYaw) + dz * std::cos(cameraYaw);
    double camY = dy * std::cos(cameraPitch) - camZ * std::sin(cameraPitch);
    depth = dy * std::sin(cameraPitch) + camZ * std::cos(cameraPitch);
    if (depth <= -5.0) return false;
    if (depth < 0.2) depth = 0.2;
    double rolledX = camX * std::cos(flipRoll) - camY * std::sin(flipRoll);
    double rolledY = camX * std::sin(flipRoll) + camY * std::cos(flipRoll);
    double focal = std::min(width, height) * 0.5 / std::tan(fieldOfView * 0.5 * PI / 180.0);
    point.x = static_cast<LONG>(width / 2.0 + centerShift + rolledX * focal / depth);
    point.y = static_cast<LONG>(height * 0.52 - rolledY * focal / depth);
    return true;
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
        POINT corners[4]{};
        POINT bottomCorners[4]{};
        double depth = 0.0;
        bool visiblePlatform = projectPoint(platform.x - platform.width / 2, platform.y,
            platform.z - platform.depth / 2, panelWidth, height, centerShift, corners[0], depth) &&
            projectPoint(platform.x + platform.width / 2, platform.y,
            platform.z - platform.depth / 2, panelWidth, height, centerShift, corners[1], depth) &&
            projectPoint(platform.x + platform.width / 2, platform.y,
            platform.z + platform.depth / 2, panelWidth, height, centerShift, corners[2], depth) &&
            projectPoint(platform.x - platform.width / 2, platform.y,
            platform.z + platform.depth / 2, panelWidth, height, centerShift, corners[3], depth);
        if (!visiblePlatform) continue;
        POINT shifted[4] = {
            {corners[0].x + panelLeft, corners[0].y}, {corners[1].x + panelLeft, corners[1].y},
            {corners[2].x + panelLeft, corners[2].y}, {corners[3].x + panelLeft, corners[3].y}
        };
        bool visibleBottom =
            projectPoint(platform.x - platform.width / 2, platform.y - 0.45,
                platform.z - platform.depth / 2, panelWidth, height, centerShift, bottomCorners[0], depth) &&
            projectPoint(platform.x + platform.width / 2, platform.y - 0.45,
                platform.z - platform.depth / 2, panelWidth, height, centerShift, bottomCorners[1], depth) &&
            projectPoint(platform.x + platform.width / 2, platform.y - 0.45,
                platform.z + platform.depth / 2, panelWidth, height, centerShift, bottomCorners[2], depth) &&
            projectPoint(platform.x - platform.width / 2, platform.y - 0.45,
                platform.z + platform.depth / 2, panelWidth, height, centerShift, bottomCorners[3], depth);
        if (!visibleBottom) continue;

        POINT shiftedBottom[4] = {
            {bottomCorners[0].x + panelLeft, bottomCorners[0].y}, {bottomCorners[1].x + panelLeft, bottomCorners[1].y},
            {bottomCorners[2].x + panelLeft, bottomCorners[2].y}, {bottomCorners[3].x + panelLeft, bottomCorners[3].y}
        };
        COLORREF sideColor = platform.floor ? RGB(35, 40, 50) : RGB(
            GetRValue(platform.color) * 2 / 3,
            GetGValue(platform.color) * 2 / 3,
            GetBValue(platform.color) * 2 / 3);
        if (!pointLight && !platform.floor) {
            sideColor = RGB(GetRValue(sideColor) / 2, GetGValue(sideColor) / 2, GetBValue(sideColor) / 2);
        }
        HBRUSH brush = CreateSolidBrush(sideColor);
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(deviceContext, brush));
        POINT side[4]{};
        side[0] = shifted[0]; side[1] = shifted[1]; side[2] = shiftedBottom[1]; side[3] = shiftedBottom[0];
        Polygon(deviceContext, side, 4);
        side[0] = shifted[1]; side[1] = shifted[2]; side[2] = shiftedBottom[2]; side[3] = shiftedBottom[1];
        Polygon(deviceContext, side, 4);
        side[0] = shifted[2]; side[1] = shifted[3]; side[2] = shiftedBottom[3]; side[3] = shiftedBottom[2];
        Polygon(deviceContext, side, 4);
        side[0] = shifted[3]; side[1] = shifted[0]; side[2] = shiftedBottom[0]; side[3] = shiftedBottom[3];
        Polygon(deviceContext, side, 4);
        SelectObject(deviceContext, oldBrush);
        DeleteObject(brush);

        brush = CreateSolidBrush(platform.color);
        oldBrush = static_cast<HBRUSH>(SelectObject(deviceContext, brush));
        Polygon(deviceContext, shifted, 4);
        SelectObject(deviceContext, oldBrush);
        DeleteObject(brush);
    }

    RestoreDC(deviceContext, savedState);
}

HWND makeMenuButton(HWND parent, const wchar_t* text, int id) {
    return CreateWindow(L"BUTTON", text, WS_CHILD | BS_PUSHBUTTON,
        0, 0, 120, 32, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandle(nullptr), nullptr);
}

void addMenuOption(HWND parent, int page, const wchar_t* title, int minusId, int plusId) {
    HWND label = CreateWindow(L"STATIC", title, WS_CHILD | SS_CENTER,
        0, 0, 180, 30, parent, nullptr, GetModuleHandle(nullptr), nullptr);
    HWND minusButton = makeMenuButton(parent, L"-", minusId);
    HWND plusButton = makeMenuButton(parent, L"+", plusId);
    menuOptions.push_back({page, minusId, plusId, label, minusButton, plusButton});
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
            if (option.minusId == 3311) { name = L"Jump m"; value = configuredJump; }
            if (option.minusId == 3321) {
                swprintf_s(text, L"Height: %.2f m", playerHeight);
                SetWindowText(option.label, text);
                continue;
            }
        } else if (option.page == 4) {
            if (option.minusId == 3421) {
                swprintf_s(text, L"Mortal speed: %.1f", mortalSpeed);
                SetWindowText(option.label, text);
                continue;
            }
            if (option.minusId == 3431) {
                swprintf_s(text, L"Twist speed: %.1f", twistSpeed);
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
    ShowWindow(backButton, page != 0 && paused ? SW_SHOW : SW_HIDE);
    ShowWindow(pointLightButton, page == 2 && paused ? SW_SHOW : SW_HIDE);
    ShowWindow(showFpsButton, page == 2 && paused ? SW_SHOW : SW_HIDE);
    for (const MenuOption& option : menuOptions) {
        bool visible = paused && page == option.page;
        ShowWindow(option.label, visible ? SW_SHOW : SW_HIDE);
        ShowWindow(option.minusButton, visible ? SW_SHOW : SW_HIDE);
        ShowWindow(option.plusButton, visible ? SW_SHOW : SW_HIDE);
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
    if (commandId == 3311 || commandId == 3312) configuredJump = std::clamp(configuredJump + direction, 1, 20);
    if (commandId == 3321 || commandId == 3322) {
        playerHeight = std::clamp(playerHeight + direction * 0.05, 1.40, 2.20);
            if (gameWindow) {
            playerY = 0.35 + playerHeight * 0.5;
            verticalSpeed = 0.0;
        }
    }
    if (commandId == 3421 || commandId == 3422) {
        mortalSpeed = std::clamp(mortalSpeed + direction * 0.1, 0.5, 1.1);
            if (activeFlipMask != 0) flipDuration = 1.0 / mortalSpeed;
    }
    if (commandId == 3431 || commandId == 3432) {
        twistSpeed = std::clamp(twistSpeed + direction * 0.1, 0.5, 1.1);
            if (activeFlipMask != 0) flipDuration = 1.0 / twistSpeed;
    }
    if (commandId >= 3101 && commandId <= 3152) resetGame();
    updateMenuLabels();
    if ((commandId == 3231 || commandId == 3232) && gameWindow) {
        SetTimer(gameWindow, 1, std::max(1, 1000 / fpsLimit), nullptr);
    }
    if (gameWindow) InvalidateRect(gameWindow, nullptr, FALSE);
}

int menuHeightForPage(int page) {
    if (page == 1) return 310;
    if (page == 2) return 230;
    if (page == 3) return 230;
    if (page == 4) return 230;
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
    MoveWindow(mapButton, center - 210, rootTop + 48, 95, 32, TRUE);
    MoveWindow(graphicsButton, center - 105, rootTop + 48, 95, 32, TRUE);
    MoveWindow(playerButton, center, rootTop + 48, 95, 32, TRUE);
    MoveWindow(flipsButton, center + 105, rootTop + 48, 95, 32, TRUE);
    MoveWindow(backButton, center - 45, menuTop + 8, 90, 32, TRUE);
    MoveWindow(pointLightButton, center - 160, menuTop + 48, 120, 32, TRUE);
    MoveWindow(showFpsButton, center + 40, menuTop + 48, 120, 32, TRUE);

    int optionIndex = 0;
    int optionTop = menuTop + ((menuPage == 2) ? 90 : 48);
    for (const MenuOption& option : menuOptions) {
        if (option.page != menuPage) continue;
        int y = optionTop + optionIndex++ * 38;
        MoveWindow(option.label, center - 90, y, 180, 30, TRUE);
        MoveWindow(option.minusButton, center - 145, y, 45, 30, TRUE);
        MoveWindow(option.plusButton, center + 100, y, 45, 30, TRUE);
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
        (menuPage == 1 ? L"MAPA" : menuPage == 2 ? L"GRAFICOS" : menuPage == 3 ? L"JUGADOR" : L"MORTALES");
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
        addMenuOption(window, 3, L"Jump m", 3311, 3312);
        addMenuOption(window, 3, L"Height", 3321, 3322);
        addMenuOption(window, 4, L"Mortal speed", 3421, 3422);
        addMenuOption(window, 4, L"Twist speed", 3431, 3432);
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
        if (!keyState[wParam < 256 ? wParam : 0]) {
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
            if (LOWORD(wParam) >= 3101 && LOWORD(wParam) <= 3412) {
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
            rotationX += (currentMouse.y - lastMouse.y) * 0.01;
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
        0,
        className,
        L"Esfera de motas clickable",
        WS_OVERLAPPEDWINDOW,
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
    SetTimer(window, 1, std::max(1, 1000 / fpsLimit), nullptr);
    SetFocus(window);

    MSG message{};
    while (GetMessage(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
    return static_cast<int>(message.wParam);
}
