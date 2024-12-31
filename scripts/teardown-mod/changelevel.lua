local globals = {}

local function DeleteLevel(name)
    local entities = FindEntities(name, true)
    for i = 1, #entities do
        Delete(entities[i])
    end
end

local function SpawnLevel(name)
    globals.curr_level = name
    local mapT = Transform(Vec(0.0, 0.0, 0.0), QuatEuler(0, 0, 0))
    Spawn("levels/" .. globals.curr_level .. ".xml", mapT, true)
    globals.changelevel_triggers = FindTriggers("changelevel", true)

    local environment = FindEntity("env", true, "environment")

    -- DebugPrint("environment = " .. environment)
    local skybox = GetTagValue(environment, "tag_skybox")
    local sunBrightness = 6
    local skyboxbrightness = 1 -- GetTagValue(environment, "tag_skyboxbrightness")
    local sunColorTintR = tonumber(GetTagValue(environment, "tag_sunColorTintR"))
    local sunColorTintG = tonumber(GetTagValue(environment, "tag_sunColorTintG"))
    local sunColorTintB = tonumber(GetTagValue(environment, "tag_sunColorTintB"))
    local sunDirX = tonumber(GetTagValue(environment, "tag_sunDirX"))
    local sunDirY = tonumber(GetTagValue(environment, "tag_sunDirY"))
    local sunDirZ = tonumber(GetTagValue(environment, "tag_sunDirZ"))

    local fogColorR, fogColorG, fogColorB = 0.0, 0.0, 0.0
    local fogParamsX, fogParamsY, fogParamsZ, fogParamsW = 20, 120, 0, 0

    local exposureX, exposureY = 1, 5
    local ambience = "outdoor/night.ogg"
    local nightlight = false
    local gamma = 0.9
    local bloom = 1.5
    local rain = 0.0

    if sunColorTintR == 0 and sunColorTintG == 0 and sunColorTintB == 0 then
        skybox = "cloudy.dds"
        skyboxbrightness = 0.05
        sunBrightness = 0
    end

    SetEnvironmentProperty("skybox", skybox)
    SetEnvironmentProperty("sunBrightness", sunBrightness)
    SetEnvironmentProperty("sunDir", sunDirX, sunDirY, -sunDirZ)
    SetEnvironmentProperty("skyboxbrightness", skyboxbrightness)
    SetEnvironmentProperty("skyboxrot", -90)
    SetEnvironmentProperty("fogColor", fogColorR, fogColorG, fogColorB)
    SetEnvironmentProperty("fogParams", fogParamsX, fogParamsY, fogParamsZ, fogParamsW)
    SetEnvironmentProperty("exposure", exposureX, exposureY)
    SetEnvironmentProperty("ambience", ambience)
    SetEnvironmentProperty("nightlight", nightlight)
    SetEnvironmentProperty("sunFogScale", 0)
    SetEnvironmentProperty("rain", rain)
    SetPostProcessingProperty("gamma", gamma)
    SetPostProcessingProperty("bloom", bloom)
end

local function TriggerLoadLevel(trigger)
    local levelName = GetTagValue(trigger, "map")
    if levelName ~= globals.curr_level then
        globals.prev_level = globals.curr_level
        globals.curr_level = levelName

        local landmarkName = GetTagValue(trigger, "landmark")
        local srcLandmark = FindLocation("targetname_" .. landmarkName)
        local locT = GetLocationTransform(srcLandmark)
        local localT = TransformToLocalTransform(locT, GetPlayerTransform(true))
        local localVel = TransformToLocalVec(locT, GetPlayerVelocity())
        DeleteLevel(globals.prev_level)
        SpawnLevel(globals.curr_level)
        local dstLandmark = FindLocation("targetname_" .. landmarkName)
        local dstLandmarkT = GetLocationTransform(dstLandmark)
        local worldT = TransformToParentTransform(dstLandmarkT, localT)
        local worldVel = TransformToParentVec(dstLandmarkT, localVel)
        SetPlayerTransform(worldT, true)
        SetPlayerVelocity(worldVel)
    end
end

local function VecDiv(a, b)
    return Vec(a[1] / b[1], a[2] / b[2], a[3] / b[3])
end
local function VecMin(a, b)
    return Vec(math.min(a[1], b[1]), math.min(a[2], b[2]), math.min(a[3], b[3]))
end
local function VecMax(a, b)
    return Vec(math.max(a[1], b[1]), math.max(a[2], b[2]), math.max(a[3], b[3]))
end

local function RaycastBox(rayPos, rayDir, boxMin, boxMax)
    local tMin = VecDiv(VecSub(boxMin, rayPos), rayDir)
    local tMax = VecDiv(VecSub(boxMax, rayPos), rayDir)
    local t1 = VecMin(tMin, tMax)
    local t2 = VecMax(tMin, tMax)
    local tNear = math.max(math.max(t1[1], t1[2]), math.max(t1[3], 0.0))
    local tFar = math.min(math.min(t2[1], t2[2]), t2[3])
    return tNear < tFar
end

local function DebugChangeLevelTrigger(trigger)
    local pMin, pMax = GetTriggerBounds(trigger)

    local _, rayPos, _, rayDir = GetPlayerAimInfo(GetPlayerEyeTransform().pos)
    local looking_at = RaycastBox(rayPos, rayDir, pMin, pMax)

    local p0 = Vec(pMin[1], pMin[2], pMin[3]);
    local p1 = Vec(pMax[1], pMin[2], pMin[3]);
    local p2 = Vec(pMax[1], pMax[2], pMin[3]);
    local p3 = Vec(pMin[1], pMax[2], pMin[3]);
    local p4 = Vec(pMin[1], pMin[2], pMax[3]);
    local p5 = Vec(pMax[1], pMin[2], pMax[3]);
    local p6 = Vec(pMax[1], pMax[2], pMax[3]);
    local p7 = Vec(pMin[1], pMax[2], pMax[3]);

    local landmarkName = GetTagValue(trigger, "landmark")
    local landmark = FindLocation("targetname_" .. landmarkName)
    local r = 1.0
    local g = 0.2
    local b = 0.2
    if landmark ~= 0 then
        local landmarkT = GetLocationTransform(landmark)
        DebugTransform(landmarkT)
        if looking_at then
            r = 0.2
            g = 1.0
            b = 0.2
        else
            r = 0.2
            g = 0.2
            b = 1.0
        end
        DebugLine(pMin, landmarkT.pos, 0.5, 0.5, 0.5);
    end

    DebugLine(p0, p1, r, g, b);
    DebugLine(p1, p2, r, g, b);
    DebugLine(p2, p3, r, g, b);
    DebugLine(p3, p0, r, g, b);
    DebugLine(p4, p5, r, g, b);
    DebugLine(p5, p6, r, g, b);
    DebugLine(p6, p7, r, g, b);
    DebugLine(p7, p4, r, g, b);
    DebugLine(p0, p4, r, g, b);
    DebugLine(p1, p5, r, g, b);
    DebugLine(p2, p6, r, g, b);
    DebugLine(p3, p7, r, g, b);
end

function init()
    SpawnLevel("c1a0")
    globals.needs_tp = true
    globals.last_changelevel_time = GetTime()
    globals.level_load_trigger = nil
    globals.loading = false
end

function tick()
    if globals.needs_tp then
        -- move player to the first level Spawn
        local spawnT = GetLocationTransform(FindLocation("playerspawn"))
        SetPlayerTransform(spawnT, true)
        globals.needs_tp = false
    end

    -- local playerT = GetPlayerTransform(true)
    -- DebugWatch("player trn", playerT)
    DebugWatch("current level", globals.curr_level)

    if InputDown('r') then
        DeleteLevel(globals.curr_level)
        SpawnLevel(globals.curr_level)
    end

    DebugWatch("hovering trigger", nil)

    for trigger_i = 1, #globals.changelevel_triggers do
        local trigger = globals.changelevel_triggers[trigger_i]
        DebugChangeLevelTrigger(trigger)

        local pMin, pMax = GetTriggerBounds(trigger)
        local _, rayPos, _, rayDir = GetPlayerAimInfo(GetPlayerEyeTransform().pos)
        local hovering_trigger = RaycastBox(rayPos, rayDir, pMin, pMax)
        local trigger_override = InputDown('e') and hovering_trigger

        if hovering_trigger then
            local dst_level_name = GetTagValue(trigger, "map")
            DebugWatch("hovering trigger", dst_level_name)
        end

        if trigger_override or IsPointInTrigger(trigger, GetPlayerTransform().pos) then
            local time = GetTime()
            local time_since_last = time - globals.last_changelevel_time
            if time_since_last > 1 then
                globals.level_load_trigger = trigger
                globals.last_changelevel_time = GetTime()
            end
        end
    end

    if globals.loading and globals.level_load_trigger then
        TriggerLoadLevel(globals.level_load_trigger)
        globals.loading = false
        globals.level_load_trigger = nil
    end
end

local function DrawLoading()
    UiPush()
    UiTranslate(UiWidth() / 2, UiHeight() / 2 - 20)
    UiAlign("center middle")
    UiFont("regular.ttf", 10)
    UiColor(1.0, 1.0, 0.1)
    UiTextOutline(0, 0, 0, 1, 0.2)
    UiText("LOADING...")
    UiPop()
end

local function DrawChapterTitle(name, animTime)
    UiPush()
    UiFont("bold.ttf", 24)
    local w, h = UiGetTextSize(name)
    local anchorX = UiWidth() / 2 - w / 2
    local anchorY = UiHeight() * 2 / 3 - h / 2
    UiTranslate(anchorX, anchorY)
    local charN = math.floor(#name * math.min(animTime, 1.0) + 0.5)
    for i = 1, charN do
        local c = name:sub(i,i)
        local factor = math.min(1, ((#name * animTime + 0.5) - i) / #name * 2)
        local colA = Vec(1, 0.5, 0)
        local colB = Vec(1, 1, 1)
        local col = VecAdd(VecScale(colA, 1-factor), VecScale(colB, factor))
        UiColor(col[1], col[2], col[3], 0.7)
        UiText(c)
        local cw, _ = UiGetTextSize(c)
        UiTranslate(cw, 0)
    end
    UiPop()
end

local function TryDrawChapterTitle(first_level, name, time)
    if globals.curr_level == first_level and time - globals.last_changelevel_time < 2 then
        DrawChapterTitle(name, time - globals.last_changelevel_time)
    end
end

function draw()
    if globals.level_load_trigger then
        DrawLoading()
        globals.loading = true
    end

    local time = GetTime()

    -- TODO: Add text for the intro sequence
    TryDrawChapterTitle("c1a0", "ANOMALOUS MATERIALS", time)
    TryDrawChapterTitle("c1a0c", "UNFORESEEN CONSEQUENCES", time)
    TryDrawChapterTitle("c1a2", "OFFICE COMPLEX", time)
    TryDrawChapterTitle("c1a3", "\"WE'VE GOT HOSTILES\"", time)
    TryDrawChapterTitle("c1a4", "BLAST PIT", time)
    TryDrawChapterTitle("c2a1", "POWER UP", time)
    TryDrawChapterTitle("c2a2", "ON A RAIL", time)
    TryDrawChapterTitle("c2a3", "APPREHENSION", time)
    TryDrawChapterTitle("c2a4", "RESIDUE PROCESSING", time)
    TryDrawChapterTitle("c2a4d", "QUESTIONABLE ETHICS", time)
    TryDrawChapterTitle("c2a5", "SURFACE TENSION", time)
    TryDrawChapterTitle("c3a1", "\"FORGET ABOUT FREEMAN!\"", time)
    TryDrawChapterTitle("c3a2e", "LAMBDA CORE", time)
    TryDrawChapterTitle("c4a1", "XEN", time)
    TryDrawChapterTitle("c4a2", "GONARCH'S LAIR", time)
    TryDrawChapterTitle("c4a1a", "INTERLOPER", time)
    TryDrawChapterTitle("c4a3", "NIHILANTH", time)
end
