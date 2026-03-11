#include <string>
#include <cstring>
#include <iostream>

#include <SDL3/SDL.h>

#include "World.hh"

constexpr int keyCountMax = 512;
int keyCount = 0;
bool prevKeysState[keyCountMax];
const bool *keysState;

bool isKeyPressed(SDL_Scancode code) {
    return keysState[code];
}

bool isKeyJustPressed(SDL_Scancode code) {
    return keysState[code] && !prevKeysState[code];
}

World* getWorld() {
    static World world;
    return &world;
}

struct Actor {
    float energy; // energy to make a move
};

struct Player {
};

PropertyHandle posProp = getWorld()->addProperty("pos", sizeof(SDL_FPoint));
PropertyHandle colorProp = getWorld()->addProperty("color", sizeof(SDL_Color));
PropertyHandle playerProp = getWorld()->addProperty("player", sizeof(Player));
PropertyHandle actorProp = getWorld()->addProperty("actor", sizeof(Actor));
PropertyHandle lifetimeProp = getWorld()->addProperty("lifetime", sizeof(uint64_t));
PropertyHandle removeProp = getWorld()->addProperty("removeFlag", 0); // why zero sized props work?

World& world = *getWorld();

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    auto playerEnt = world.newEntity();
    world.property(posProp).addForEntity(playerEnt, SDL_FPoint{0, 0});
    world.property(colorProp).addForEntity(playerEnt, SDL_Color{255, 0, 0, 255});

    auto e = world.newEntity();
    world.property(posProp).addForEntity(e, SDL_FPoint{100, 100});
    world.property(colorProp).addForEntity(e, SDL_Color{255, 255, 0, 255});
    world.property(lifetimeProp).addForEntity(e, static_cast<uint64_t>(3 * 1000));

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_init: %s", SDL_GetError());
        return 1;
    }
    uint64_t prev = SDL_GetTicks();
    SDL_Window* win = nullptr;
    SDL_Renderer* rend = nullptr;
    if (!SDL_CreateWindowAndRenderer(
        "Hellope", 800, 600, SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE, &win, &rend)) {
        SDL_Log("SDL_CreateWindowAndRenderer: %s", SDL_GetError());
        return 1;
    }

    keysState = SDL_GetKeyboardState(&keyCount);
    if (!keysState) {
        SDL_Log("SDL_GeyKeyboardState: %s", SDL_GetError());
        return 1;
    }
    if (keyCount > keyCountMax) {
        SDL_Log("Too many keys");
        return 1;
    }

    bool running = true;
    while (running) {
        SDL_Event ev = {};
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_EVENT_QUIT:
                running = false;
            }
        }

        keysState = SDL_GetKeyboardState(&keyCount);
        if (isKeyJustPressed(SDL_SCANCODE_Q)) {
            running = false;
        }
        uint64_t now = SDL_GetTicks();
        uint64_t passed = now - prev;

        auto lifetimeIt = world.property(lifetimeProp).getIterator<uint64_t>();
        while (lifetimeIt.hasNext()) {
            auto [time, eIdx] = lifetimeIt.get();
            // UNSIGNED, be careful with overflow
            if (passed >= *time) {
                world.property(removeProp).addForEntity(eIdx, "");
            } else {
                *time -= passed;
            }
            lifetimeIt.next();
        }

        auto playerPos = world.property(posProp).getForEntity<SDL_FPoint>(playerEnt);
        SDL_assert(playerPos);
        if (isKeyJustPressed(SDL_SCANCODE_S)) {
            playerPos->y += 1;
        }

        std::memcpy(prevKeysState, keysState, keyCount * sizeof(keysState[0]));

// Render
        SDL_SetRenderDrawColor(rend, 255, 255, 255, 255);
        SDL_RenderClear(rend);
        auto posIterator = world.property(posProp).getIterator<SDL_FPoint>();
        for (; posIterator.hasNext(); posIterator.next()) {
            auto [pos, eIdx] = posIterator.get();
            const SDL_Color* color = world.property(colorProp).getForEntity<SDL_Color>(eIdx);
            if (!color) continue;
            SDL_SetRenderDrawColor(rend, color->r, color->g, color->b, color->a);
            const SDL_FRect rect = {pos->x, pos->y, 100, 100};
            SDL_RenderFillRect(rend, &rect);
        }
        SDL_RenderPresent(rend);

        auto removeIt = world.property(removeProp).getIterator<void>();
        while (removeIt.hasNext()) {
            EntityIdx idx = removeIt.get();
            world.deleteEntity(idx);
            removeIt.next();
        }
        prev = now;
    }
}

