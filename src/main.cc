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

auto posProp = getWorld()->addProperty<SDL_FPoint>("pos");
auto colorProp = getWorld()->addProperty<SDL_Color>("color");
auto playerProp = getWorld()->addProperty<Player>("player");
auto actorProp = getWorld()->addProperty<Actor>("actor");
auto lifetimeProp = getWorld()->addProperty<uint64_t>("lifetime");
auto removeProp = getWorld()->addProperty<void>("removeFlag"); // why zero sized props work?

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

        auto lifetimeIt = world.getItForProp(lifetimeProp);
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

        auto playerPos = world.getPropForEntt(posProp, playerEnt); 
        SDL_assert(playerPos);
        if (isKeyJustPressed(SDL_SCANCODE_S)) {
            playerPos->y += 1;
        }

        std::memcpy(prevKeysState, keysState, keyCount * sizeof(keysState[0]));

// Render
        SDL_SetRenderDrawColor(rend, 255, 255, 255, 255);
        SDL_RenderClear(rend);
        auto posIterator = world.getItForProp(posProp);
        for (; posIterator.hasNext(); posIterator.next()) {
            auto [pos, eIdx] = posIterator.get();
            const SDL_Color* color = world.getPropForEntt(colorProp, eIdx); 
            if (!color) continue;
            SDL_SetRenderDrawColor(rend, color->r, color->g, color->b, color->a);
            const SDL_FRect rect = {pos->x, pos->y, 100, 100};
            SDL_RenderFillRect(rend, &rect);
        }
        SDL_RenderPresent(rend);

        auto removeIt = world.getItForProp(removeProp);
        while (removeIt.hasNext()) {
            EntityIdx idx = removeIt.get();
            world.deleteEntity(idx);
            removeIt.next();
        }
        prev = now;
    }
}

