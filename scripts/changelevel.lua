local globals = {}

function DeleteLevel(name)
    local entities = FindEntities(name, true)
    for i = 1, #entities do
        Delete(entities[i])
    end
end

function TriggerLoadLevel(trigger)
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
        Spawn("levels/" .. globals.curr_level .. ".xml", mapT, true)
        globals.changelevel_triggers = FindTriggers("changelevel", true)

        local dstLandmark = FindLocation("targetname_" .. landmarkName)
        local dstLandmarkT = GetLocationTransform(dstLandmark)
        local worldT = TransformToParentTransform(dstLandmarkT, localT)
        local worldVel = TransformToParentVec(dstLandmarkT, localVel)
        SetPlayerTransform(worldT, true)
        SetPlayerVelocity(worldVel)
    end
end

function VecDiv(a, b)
    return Vec(a[1] / b[1], a[2] / b[2], a[3] / b[3])
end
function VecMin(a, b)
    return Vec(math.min(a[1], b[1]), math.min(a[2], b[2]), math.min(a[3], b[3]))
end
function VecMax(a, b)
    return Vec(math.max(a[1], b[1]), math.max(a[2], b[2]), math.max(a[3], b[3]))
end

function RaycastBox(rayPos, rayDir, boxMin, boxMax)
    local tMin = VecDiv(VecSub(boxMin, rayPos), rayDir)
    local tMax = VecDiv(VecSub(boxMax, rayPos), rayDir)
    local t1 = VecMin(tMin, tMax)
    local t2 = VecMax(tMin, tMax)
    local tNear = math.max(math.max(t1[1], t1[2]), math.max(t1[3], 0.0))
    local tFar = math.min(math.min(t2[1], t2[2]), t2[3])
    return tNear < tFar
end

function DebugChangeLevelTrigger(trigger)
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
    mapT = Transform(Vec(0.0, 0.0, 0.0), QuatEuler(0, 0, 0), Vec(1, 1, 1))
    globals.curr_level = "c1a0"
    globals.prev_level = "none"
    globals.needs_tp = true
    Spawn("levels/" .. globals.curr_level .. ".xml", mapT, true)
    globals.changelevel_triggers = FindTriggers("changelevel", true)
    globals.last_changelevel_time = GetTime()
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

    for trigger_i = 1, #globals.changelevel_triggers do
        local trigger = globals.changelevel_triggers[trigger_i]
        DebugChangeLevelTrigger(trigger)

        local trigger_override = false
        if InputDown('e') then
            local pMin, pMax = GetTriggerBounds(trigger)
            local _, rayPos, _, rayDir = GetPlayerAimInfo(GetPlayerEyeTransform().pos)
            trigger_override = RaycastBox(rayPos, rayDir, pMin, pMax)
        end

        if trigger_override or IsPointInTrigger(trigger, GetPlayerTransform().pos) then
            local time = GetTime()
            local time_since_last = time - globals.last_changelevel_time
            if time_since_last > 1 then
                TriggerLoadLevel(trigger)
                globals.last_changelevel_time = GetTime()
            end
        end
    end
end
