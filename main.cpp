#include <string>
#include "raylib.h"
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <vector>

const double G = 6.67430e-11;
double timeScale = 1.0;  // 1x = real time
const double maxStep = 10.0;

// Zoom: meters per pixel (smaller = zoom in, larger = zoom out)
double metersPerPixel = 2.0e6; // start zoom (2,000 km per pixel)

// Camera center in world meters (what the screen center looks at)
struct Vec2d { double x, y; };

// Spawn mode states
enum SpawnState { SPAWN_IDLE, SPAWN_PLACING, SPAWN_AIMING };
SpawnState spawnState = SPAWN_IDLE;
Vec2d spawnPos = {0, 0};           // World position for new probe
char speedInput[32] = "1000";      // Speed input buffer
int speedInputLen = 4;             // Current length of input
bool simulationPaused = false;

// Probe trail
std::vector<Vec2d> probeTrail;
const int MAX_TRAIL_POINTS = 10000;
double trailTimer = 0.0;
const double TRAIL_INTERVAL = 0.5;  // Record position every 0.5 sim seconds

// Probe state
bool probeAlive = true;
Vec2d cameraCenter = {0.0, 0.0};


//BODY OBJECT. this is what evey object in the sim is made of, including the probe
struct Body {
    const char* name;
    double mass;    // kg
    double radius;  // m
    Vec2d pos;      // m
    Vec2d vel;      // m/s
    bool affectedByGravity = true;
    bool affectsOthers = true;
    Color color;
};


//earth, moon, and probe definitions
//-----------------------------------------------------
Body earth = {
    "Earth",
    5.972e24,
    6.371e6,
    {0.0, 0.0},
    {0.0, 0.0},
    true,
    true,
    BLUE
};

Body moon = {
    "Moon",
    7.34767309e22,
    1.7371e6,
    {384.4e6, 0.0},
    {0.0, 1022.0},
    true,
    true,
    LIGHTGRAY
};

Body probe = {
    "Probe",
    1000.0,
    2.0e5, // 200 km (will be clamped for visibility)
    {6.371e6 + 400e3, 0.0},
    {0.0, 0.0},
    true,
    false, // probe does not affect others
    RED
};
//-----------------------------------------------------


// Convert world position (meters) to screen position (pixels)
Vector2 ToScreen(Vec2d worldPos, int screenWidth, int screenHeight, double metersPerPixel, Vec2d cameraCenter)
{
    // Translate so cameraCenter is at screen center
    double relX = worldPos.x - cameraCenter.x;
    double relY = worldPos.y - cameraCenter.y;

    return {
        (float)(screenWidth / 2.0 + relX / metersPerPixel),
        (float)(screenHeight / 2.0 - relY / metersPerPixel)
    };
}

//does the opposite of ToScreen
// Convert screen position (pixels) to world position (meters)
Vec2d ToWorld(Vector2 screenPos, int screenWidth, int screenHeight, double metersPerPixel, Vec2d cameraCenter)
{
    // Convert screen position to world coordinates
    double relX = (screenPos.x - screenWidth / 2.0) * metersPerPixel;
    double relY = (screenHeight / 2.0 - screenPos.y) * metersPerPixel;

    return {
        cameraCenter.x + relX,
        cameraCenter.y + relY
    };
}


//main function -------------------------------------------------------------------------------------
int main()
{
    const int screenWidth = 1200;
    const int screenHeight = 800;

    InitWindow(screenWidth, screenHeight, "Earth–Moon Simulator");
    SetTargetFPS(60);

    // Set probe to circular-ish orbit around Earth (ignoring Moon)
    double r = probe.pos.x; // since y=0 here
    double vCirc = sqrt(G * earth.mass / r);
    probe.vel = {0.0, vCirc};

    while (!WindowShouldClose())
    {
        double realDt = GetFrameTime();

        // Zoom control (mouse wheel) 
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f)
        {
            double zoomFactor = 1.15;
            if (wheel > 0) metersPerPixel /= zoomFactor; // zoom in
            if (wheel < 0) metersPerPixel *= zoomFactor; // zoom out

            // clamp zoom range
            if (metersPerPixel < 1.0) metersPerPixel = 1.0;
            if (metersPerPixel > 1.0e9) metersPerPixel = 1.0e9;
        }

        // Pan control (WASD) 
        // Pan speed scales with zoom so it "feels" consistent.
        double panSpeed = metersPerPixel * 600.0; // meters/second
        if (IsKeyDown(KEY_A)) cameraCenter.x -= panSpeed * realDt;
        if (IsKeyDown(KEY_D)) cameraCenter.x += panSpeed * realDt;
        if (IsKeyDown(KEY_W)) cameraCenter.y += panSpeed * realDt;
        if (IsKeyDown(KEY_S)) cameraCenter.y -= panSpeed * realDt;

        // Timescale control (arrow keys) 
        if (spawnState == SPAWN_IDLE)
        {
            if (IsKeyPressed(KEY_UP))
            {
                timeScale *= 2.0;
                if (timeScale > 100000.0) timeScale = 100000.0;
            }
            if (IsKeyPressed(KEY_DOWN))
            {
                timeScale /= 2.0;
                if (timeScale < 0.125) timeScale = 0.125;
            }
        }

        // Spawn Probe Button 
        Rectangle spawnButton = {(float)screenWidth - 150, 10, 140, 40};
        bool buttonHovered = CheckCollisionPointRec(GetMousePosition(), spawnButton);

        if (spawnState == SPAWN_IDLE)
        {
            if (buttonHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                spawnState = SPAWN_PLACING;
                simulationPaused = true;
            }
        }
        else if (spawnState == SPAWN_PLACING)
        {
            // Click to place probe position
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !buttonHovered)
            {
                spawnPos = ToWorld(GetMousePosition(), screenWidth, screenHeight, metersPerPixel, cameraCenter);
                spawnState = SPAWN_AIMING;
            }
            // ESC to cancel
            if (IsKeyPressed(KEY_ESCAPE))
            {
                spawnState = SPAWN_IDLE;
                simulationPaused = false;
            }
        }
        else if (spawnState == SPAWN_AIMING)
        {
            // Handle speed input
            int key = GetCharPressed();
            while (key > 0)
            {
                if ((key >= '0' && key <= '9') || key == '.')
                {
                    if (speedInputLen < 31)
                    {
                        speedInput[speedInputLen] = (char)key;
                        speedInputLen++;
                        speedInput[speedInputLen] = '\0';
                    }
                }
                key = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && speedInputLen > 0)
            {
                speedInputLen--;
                speedInput[speedInputLen] = '\0';
            }

            // Enter to confirm and spawn
            if (IsKeyPressed(KEY_ENTER))
            {
                double speed = atof(speedInput);
                if (speed < 0) speed = 0;

                // Get direction from spawn point to mouse
                Vector2 spawnScreen = ToScreen(spawnPos, screenWidth, screenHeight, metersPerPixel, cameraCenter);
                Vector2 mouse = GetMousePosition();
                float dx = mouse.x - spawnScreen.x;
                float dy = mouse.y - spawnScreen.y;
                float len = sqrtf(dx * dx + dy * dy);

                if (len > 1.0f)
                {
                    double dirX = dx / len;
                    double dirY = -dy / len;  // Invert Y for world coords
                    probe.pos = spawnPos;
                    probe.vel.x = dirX * speed;
                    probe.vel.y = dirY * speed;
                }
                else
                {
                    // No direction, just place with zero velocity
                    probe.pos = spawnPos;
                    probe.vel.x = 0;
                    probe.vel.y = 0;
                }

                // Clear trail and reset probe state
                probeTrail.clear();
                trailTimer = 0.0;
                probeAlive = true;

                spawnState = SPAWN_IDLE;
                simulationPaused = false;
            }

            // ESC to cancel
            if (IsKeyPressed(KEY_ESCAPE))
            {
                spawnState = SPAWN_IDLE;
                simulationPaused = false;
            }
        }

        // Physics time step (only if not paused) 
        if (!simulationPaused)
        {
        double dt = realDt * timeScale;

        int steps = 1;
        if (dt > maxStep) steps = (int)ceil(dt / maxStep);
        double h = dt / steps;

        for (int s = 0; s < steps; s++)
        {
            //MOON acceleration from EARTH
            double dxME = earth.pos.x - moon.pos.x;
            double dyME = earth.pos.y - moon.pos.y;
            double d2ME = dxME * dxME + dyME * dyME;
            double dME = sqrt(d2ME);

            double invD3ME = 1.0 / (dME * dME * dME + 1e-12);
            double axMoon = G * earth.mass * dxME * invD3ME;
            double ayMoon = G * earth.mass * dyME * invD3ME;

            moon.vel.x += axMoon * h;
            moon.vel.y += ayMoon * h;
            moon.pos.x += moon.vel.x * h;
            moon.pos.y += moon.vel.y * h;

            // PROBE acceleration from EARTH + MOON 
            if (probeAlive)
            {
                double axProbe = 0.0;
                double ayProbe = 0.0;

                // from Earth
                double dxPE = earth.pos.x - probe.pos.x;
                double dyPE = earth.pos.y - probe.pos.y;
                double d2PE = dxPE * dxPE + dyPE * dyPE;
                double dPE = sqrt(d2PE);

                double invD3PE = 1.0 / (dPE * dPE * dPE + 1e-12);
                axProbe += G * earth.mass * dxPE * invD3PE;
                ayProbe += G * earth.mass * dyPE * invD3PE;

                // from Moon
                double dxPM = moon.pos.x - probe.pos.x;
                double dyPM = moon.pos.y - probe.pos.y;
                double d2PM = dxPM * dxPM + dyPM * dyPM;
                double dPM = sqrt(d2PM);

                double invD3PM = 1.0 / (dPM * dPM * dPM + 1e-12);
                axProbe += G * moon.mass * dxPM * invD3PM;
                ayProbe += G * moon.mass * dyPM * invD3PM;

                probe.vel.x += axProbe * h;
                probe.vel.y += ayProbe * h;
                probe.pos.x += probe.vel.x * h;
                probe.pos.y += probe.vel.y * h;

                // Collision detection
                if (dPE <= earth.radius)
                {
                    probeAlive = false;  // Hit Earth
                }
                else if (dPM <= moon.radius)
                {
                    probeAlive = false;  // Hit Moon
                }

                // Record trail position periodically
                if (probeAlive)
                {
                    trailTimer += h;
                    if (trailTimer >= TRAIL_INTERVAL)
                    {
                        trailTimer = 0.0;
                        probeTrail.push_back(probe.pos);
                        if (probeTrail.size() > MAX_TRAIL_POINTS)
                        {
                            probeTrail.erase(probeTrail.begin());
                        }
                    }
                }
            }
        }


        //
        }

        // Draw
        BeginDrawing();
        ClearBackground(BLACK);

        // Earth
        Vector2 earthScreenPos = ToScreen(earth.pos, screenWidth, screenHeight, metersPerPixel, cameraCenter);
        float earthRadiusPixels = (float)(earth.radius / metersPerPixel);
        DrawCircleV(earthScreenPos, earthRadiusPixels, earth.color);

        // Moon
        Vector2 moonScreenPos = ToScreen(moon.pos, screenWidth, screenHeight, metersPerPixel, cameraCenter);
        float moonRadiusPixels = (float)(moon.radius / metersPerPixel);
        DrawCircleV(moonScreenPos, moonRadiusPixels, moon.color);

        // Probe trail (always draw trail even if probe crashed)
        if (probeTrail.size() > 1)
        {
            for (size_t i = 1; i < probeTrail.size(); i++)
            {
                Vector2 p1 = ToScreen(probeTrail[i - 1], screenWidth, screenHeight, metersPerPixel, cameraCenter);
                Vector2 p2 = ToScreen(probeTrail[i], screenWidth, screenHeight, metersPerPixel, cameraCenter);

                // Fade color from dark to bright along trail
                float alpha = (float)i / (float)probeTrail.size();
                Color trailColor = Fade(ORANGE, 0.3f + 0.7f * alpha);
                DrawLineV(p1, p2, trailColor);
            }
            // Draw line from last trail point to current probe position (only if alive)
            if (!probeTrail.empty() && probeAlive)
            {
                Vector2 lastTrail = ToScreen(probeTrail.back(), screenWidth, screenHeight, metersPerPixel, cameraCenter);
                Vector2 currentPos = ToScreen(probe.pos, screenWidth, screenHeight, metersPerPixel, cameraCenter);
                DrawLineV(lastTrail, currentPos, ORANGE);
            }
        }

        // Probe (only draw if alive)
        if (probeAlive)
        {
            Vector2 probeScreenPos = ToScreen(probe.pos, screenWidth, screenHeight, metersPerPixel, cameraCenter);
            float probeRadiusPixels = (float)(probe.radius / metersPerPixel);
            if (probeRadiusPixels < 2.0f) probeRadiusPixels = 2.0f; // keep visible
            DrawCircleV(probeScreenPos, probeRadiusPixels, probe.color);
        }

        // Draw spawn button
        Color buttonColor = buttonHovered ? DARKGREEN : DARKGRAY;
        if (spawnState != SPAWN_IDLE) buttonColor = GREEN;
        DrawRectangleRec(spawnButton, buttonColor);
        DrawRectangleLinesEx(spawnButton, 2, RAYWHITE);
        DrawText("Spawn Probe", (int)spawnButton.x + 10, (int)spawnButton.y + 12, 18, RAYWHITE);

        // Draw spawn mode UI
        if (spawnState == SPAWN_PLACING)
        {
            // Show crosshair at mouse
            Vector2 mouse = GetMousePosition();
            DrawLine((int)mouse.x - 15, (int)mouse.y, (int)mouse.x + 15, (int)mouse.y, GREEN);
            DrawLine((int)mouse.x, (int)mouse.y - 15, (int)mouse.x, (int)mouse.y + 15, GREEN);

            // Instructions
            DrawRectangle(screenWidth/2 - 150, screenHeight - 60, 300, 50, Fade(BLACK, 0.7f));
            DrawText("Click to place probe position", screenWidth/2 - 130, screenHeight - 50, 20, GREEN);
        }
        else if (spawnState == SPAWN_AIMING)
        {
            // Draw spawn position marker
            Vector2 spawnScreen = ToScreen(spawnPos, screenWidth, screenHeight, metersPerPixel, cameraCenter);
            DrawCircleV(spawnScreen, 8.0f, GREEN);
            DrawCircleLines((int)spawnScreen.x, (int)spawnScreen.y, 12.0f, GREEN);

            // Draw direction arrow to mouse
            Vector2 mouse = GetMousePosition();
            float dx = mouse.x - spawnScreen.x;
            float dy = mouse.y - spawnScreen.y;
            float len = sqrtf(dx * dx + dy * dy);

            if (len > 10.0f)
            {
                DrawLineEx(spawnScreen, mouse, 2.0f, YELLOW);
                // Arrowhead
                float nx = dx / len;
                float ny = dy / len;
                float arrowSize = 12.0f;
                Vector2 arrow1 = {mouse.x - arrowSize * (nx + ny * 0.5f), mouse.y - arrowSize * (ny - nx * 0.5f)};
                Vector2 arrow2 = {mouse.x - arrowSize * (nx - ny * 0.5f), mouse.y - arrowSize * (ny + nx * 0.5f)};
                DrawLineEx(mouse, arrow1, 2.0f, YELLOW);
                DrawLineEx(mouse, arrow2, 2.0f, YELLOW);
            }

            // Speed input box
            DrawRectangle(screenWidth/2 - 150, screenHeight - 100, 300, 90, Fade(BLACK, 0.8f));
            DrawText("Speed (m/s):", screenWidth/2 - 130, screenHeight - 90, 20, RAYWHITE);

            // Input field
            DrawRectangle(screenWidth/2 - 130, screenHeight - 65, 260, 30, DARKGRAY);
            DrawRectangleLines(screenWidth/2 - 130, screenHeight - 65, 260, 30, RAYWHITE);
            DrawText(speedInput, screenWidth/2 - 125, screenHeight - 60, 22, YELLOW);

            // Blinking cursor
            if (((int)(GetTime() * 2) % 2) == 0)
            {
                int textWidth = MeasureText(speedInput, 22);
                DrawText("_", screenWidth/2 - 125 + textWidth, screenHeight - 60, 22, YELLOW);
            }

            DrawText("Point arrow, then press ENTER", screenWidth/2 - 130, screenHeight - 30, 16, GREEN);
            DrawText("ESC to cancel", screenWidth/2 - 55, screenHeight - 12, 14, GRAY);
        }

        // Paused indicator
        if (simulationPaused)
        {
            DrawText("PAUSED", screenWidth/2 - 50, 60, 30, YELLOW);
        }

        // HUD
        DrawText(TextFormat("Zoom: %.3e m/px (wheel)", metersPerPixel), 10, 10, 18, RAYWHITE);
        DrawText("Pan: WASD (hold Shift = faster)", 10, 32, 18, RAYWHITE);
        DrawText(TextFormat("Time: %.2fx (Up/Down arrows)", timeScale), 10, 54, 18, RAYWHITE);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
