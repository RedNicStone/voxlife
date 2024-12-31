
function Gun()
    return {
        codename = "",
        displayName = "",
        lastFireTime = 0,
        baseAmmo = 0,
        category = 0,
        Body = 0,
        originalT = Transform(),
        shootT = Transform(),
        shootSnd = 0,
        shootAnimTime = 0,
        shootAnimDuration = 0.5
    }
end

function InitGun(gun)
    RegisterTool("voxlife_" .. gun.codename, gun.displayName, "MOD/tools/old/" .. gun.codename .. ".vox", gun.category)
    SetBool("game.tool.voxlife_" .. gun.codename .. ".enabled", true)
    if gun.baseAmmo < 0 then
        SetString("game.tool.voxlife_" .. gun.codename .. ".ammo.display", "")
    else
        SetInt("game.tool.voxlife_" .. gun.codename .. ".ammo", gun.baseAmmo * 100)
    end
    gun.shootSnd = LoadSound("MOD/tools/old/" .. gun.codename .. "-shoot.ogg")
end

function TickGun(gun, dt)
    gun.shootAnimTime = gun.shootAnimTime + dt
    if GetString("game.player.tool") == "voxlife_" .. gun.codename then
        if GetBool("game.player.canusetool") and InputDown("usetool") and
            (gun.baseAmmo < 0 or GetInt("game.tool.voxlife_" .. gun.codename .. ".ammo") > 0) and gun.shootAnimTime >
            gun.shootAnimDuration then
            gun.shootAnimTime = 0
            if gun.baseAmmo >= 0 then
                local ammo = GetInt("game.tool.voxlife_" .. gun.codename .. ".ammo")
                SetInt("game.tool.voxlife_" .. gun.codename .. ".ammo", ammo - 1)
            end
            local playerCameraT = GetPlayerCameraTransform()
            local direction = TransformToParentVec(playerCameraT, Vec(0, 0, -1))
            local hit, dist = QueryRaycast(playerCameraT.pos, direction, 100)
            if hit then
                local hitPos = VecAdd(playerCameraT.pos, VecScale(direction, dist))
                MakeHole(hitPos, 0.3)
            end
            PlaySound(gun.shootSnd)
        end

        local b = GetToolBody()
        if b ~= 0 then
            local shapes = GetBodyShapes(b)
            if b ~= gun.Body then
                gun.Body = b
                -- Get default transform
                gun.originalT = GetShapeLocalTransform(shapes[1])
            end
            local animT = math.min(gun.shootAnimTime / gun.shootAnimDuration, 1)
            animT = math.sin(animT * math.pi * 0.5)
            local offsetPos = VecLerp(gun.shootT.pos, Vec(0), animT)
            local offsetRos = QuatSlerp(gun.shootT.rot, QuatEuler(0, 0, 0), animT)
            SetShapeLocalTransform(shapes[1], TransformToParentTransform(gun.originalT, Transform(offsetPos, offsetRos)))
            local toolTransform = gun.toolT1
            if GetBool("game.thirdperson") then
                toolTransform = gun.toolT3
                local handRT = gun.toolHandRT
                local handLT = gun.toolHandLT
                -- if handRT then
                --     handRT = TransformToParentTransform(handRT, Transform(offsetPos, offsetRos))
                -- end
                -- if handLT then
                --     handLT = TransformToParentTransform(handLT, Transform(offsetPos, offsetRos))
                -- end
                SetToolHandPoseLocalTransform(handRT, handLT)
            end
            SetToolTransform(toolTransform)
        end
    end
end


local crowbar = Gun()
-- local glock = Gun()
-- local revolver = Gun()
local mp5 = Gun()
local shotgun = Gun()
local crossbow = Gun()

old = {}

function old:init()
    crowbar.codename = "crowbar"
    crowbar.displayName = "Crowbar"
    crowbar.baseAmmo = -1
    crowbar.category = 1
    crowbar.shootAnimDuration = 0.25
    crowbar.toolT1 = Transform(Vec(0.35, -0.5, -0.5), QuatEuler(-15, 5, 5))
    crowbar.toolT3 = Transform(Vec(0.32, -0.2, -0.2), QuatEuler(45, 0, 10))
    crowbar.toolHandLT = nil
    crowbar.toolHandRT = Transform(Vec(-0.025, 0, -0.025), QuatAxisAngle(Vec(0, 1, 0), 90.0))
    crowbar.shootT = Transform(Vec(0.0, -0.2, 0.07), QuatEuler(-30, 0, 10))
    InitGun(crowbar)

    -- glock.codename = "glock"
    -- glock.displayName = "Glock"
    -- glock.baseAmmo = 17
    -- glock.category = 2
    -- glock.shootAnimDuration = 0.3 -- secondary 0.2
    -- glock.toolT1 = Transform(VecScale(Vec(0.3, -0.6, -1), 0.65), QuatEuler(5, 2, 6))
    -- glock.toolT3 = Transform(Vec(0.1, -0.3, -0.5))
    -- glock.toolHandLT = Transform(Vec(0.0, 0.0, -0.08), QuatAxisAngle(Vec(0, 1, 0), 90.0))
    -- glock.toolHandRT = Transform(Vec(0.0, 0.0, -0.08), QuatAxisAngle(Vec(0, 1, 0), 0.0))
    -- glock.shootT = Transform(Vec(0.0, -0.2, 0.07), QuatEuler(20, 0, 0))
    -- InitGun(glock)

    -- revolver.codename = "revolver"
    -- revolver.displayName = ".357 Colt Python"
    -- revolver.baseAmmo = 8
    -- revolver.category = 2
    -- revolver.shootAnimDuration = 0.75
    -- revolver.toolT1 = Transform(VecScale(Vec(0.34, -0.5, -0.8), 0.9), QuatEuler(5, 2, 0))
    -- revolver.toolT3 = Transform(Vec(0.1, -0.3, -0.5))
    -- revolver.toolHandLT = Transform(Vec(0.0, 0.0, -0.08), QuatAxisAngle(Vec(0, 1, 0), 90.0))
    -- revolver.toolHandRT = Transform(Vec(0.0, 0.0, -0.08), QuatAxisAngle(Vec(0, 1, 0), 0.0))
    -- revolver.shootT = Transform(Vec(0.0, -0.2, 0.07), QuatEuler(40, 0, 0))
    -- InitGun(revolver)

    mp5.codename = "mp5"
    mp5.displayName = "MP5"
    mp5.baseAmmo = 25
    mp5.category = 3
    mp5.shootAnimDuration = 0.1 -- secondary 1.0
    mp5.toolT1 = Transform(VecScale(Vec(0.34, -0.5, -0.8), 0.9), QuatEuler(5, 2, 0))
    mp5.toolT3 = Transform(Vec(0.25, -0.6, -0.3))
    mp5.toolHandLT = Transform(Vec(0.0, 0.0, -0.08), QuatAxisAngle(Vec(0, 1, 0), 90.0))
    mp5.toolHandRT = Transform(Vec(0.0, 0.0, -0.08), QuatAxisAngle(Vec(0, 1, 0), 0.0))
    mp5.shootT = Transform(Vec(0.0, -0.1, 0.0), QuatEuler(3, 0, 0))
    InitGun(mp5)

    shotgun.codename = "shotgun"
    shotgun.displayName = "Shotgun"
    shotgun.baseAmmo = 8
    shotgun.category = 3
    shotgun.shootAnimDuration = 0.75
    shotgun.toolT1 = Transform(VecScale(Vec(0.34, -0.5, -0.8), 0.9), QuatEuler(5, 2, 0))
    shotgun.toolT3 = Transform(Vec(0.25, -0.6, -0.3))
    shotgun.toolHandLT = Transform(Vec(0.0, 0.0, -0.08), QuatAxisAngle(Vec(0, 1, 0), 90.0))
    shotgun.toolHandRT = Transform(Vec(0.0, 0.0, -0.08), QuatAxisAngle(Vec(0, 1, 0), 0.0))
    shotgun.shootT = Transform(Vec(0.0, -0.2, 0.07), QuatEuler(40, 0, 0))
    InitGun(shotgun)

    crossbow.codename = "crossbow"
    crossbow.displayName = "Crossbow"
    crossbow.baseAmmo = 5
    crossbow.category = 3
    crossbow.shootAnimDuration = 0.75 -- guess
    crossbow.toolT1 = Transform(VecScale(Vec(0.34, -0.5, -0.8), 0.9), QuatEuler(5, 2, 0))
    crossbow.toolT3 = Transform(Vec(0.25, -0.6, -0.3))
    crossbow.toolHandLT = Transform(Vec(0.0, 0.0, -0.08), QuatAxisAngle(Vec(0, 1, 0), 90.0))
    crossbow.toolHandRT = Transform(Vec(0.0, 0.0, -0.08), QuatAxisAngle(Vec(0, 1, 0), 0.0))
    crossbow.shootT = Transform(Vec(0.0, -0.4, 0.0), QuatEuler(0, 0, 0))
    InitGun(crossbow)

    -- -- rocketlauncher
    -- tau.shootAnimDuration = 0.2 -- secondary charge
    -- gluon.shootAnimDuration = 0.0 -- continuous
    -- hivehand.shootAnimDuration = 0.25 -- 0.1 secondary
end

function old:tick(dt)
    TickGun(crowbar, dt)
    -- TickGun(glock, dt)
    -- TickGun(revolver, dt)
    TickGun(mp5, dt)
    TickGun(shotgun, dt)
    TickGun(crossbow, dt)
end

function old:draw(dt)
end
