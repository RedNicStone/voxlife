-- This script will run on all levels when mod is active.
-- Modding documentation: http://teardowngame.com/modding
-- API reference: http://teardowngame.com/modding/api.html

#include "tools/glock/main.lua"
#include "tools/revolver/main.lua"
#include "tools/tau/main.lua"
#include "tools/old.lua"

local tools = {
    old,
    glock,
    revolver,
    tau,
}

function init()
    for i = 1, #tools do
        tools[i]:init()
    end
end

function tick(dt)
    for i = 1, #tools do
        tools[i]:tick(dt)
    end
end

function update(dt)
end

function draw(dt)
    for i = 1, #tools do
        tools[i]:draw()
    end
end
