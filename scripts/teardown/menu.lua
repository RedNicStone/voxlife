
local globals = {}
local previous_image_name = ""

function init()
    globals.audio = {}
    globals.audio.intro_stinger = LoadSound("MOD/assets/audio/intro_stinger.ogg")
    globals.has_intro_begun = false
    globals.has_intro_finished = false
    
    SetEnvironmentProperty("ambience", "")
end

function doIntro()
    if globals.has_intro_finished then
        return false
    end
    
    if not globals.has_intro_begun then
        PlaySoundForUser(globals.audio.intro_stinger, 0)
        globals.has_intro_begun = true
    end

    local t = GetTime()
    local frame = t * 15;  -- 15 frames per second

    if frame < 150 then
        SetBool("game.disableinput", true)
        SetBool("hud.disable", true)

        local image_path = "MOD/assets/image_sequence/intro/"
        local image_name = "valve_" .. string.format("%04d", frame + 1) .. ".png"
        UiPush()
            UiImageBox(image_path .. image_name, UiWidth(), UiHeight())
        UiPop()
        
        if not previous_image_name == "" then
            UiUnloadImage(previous_image_name)
        end

        previous_image_name = image_name

        return true
    end

    globals.has_intro_finished = true
    UiUnloadImage(previous_image_name)
    UnloadSound(globals.audio.intro_stinger)

    return false
end

local previous_highlight = {}
function doMenuEntry(dt, title, description, disabled)
    local title_font_size = UiHeight() * 0.04
    UiFont("MOD/assets/fonts/FiraSans-Medium.ttf", title_font_size)

    DebugWatch("mouse", mouse_x)
    DebugWatch("cursor", cursor_x)
    DebugWatch("hovering", is_over_text)

    UiPush()
        if not disabled then
            local mouse_x, mouse_y = UiGetMousePos()
            local title_size_x, title_size_y = UiGetTextSize(title)
        
            local is_over_text = mouse_x > -UiWidth() * 0.02 and mouse_y > -title_size_y - UiHeight() * 0.01 and mouse_x < UiWidth() * 0.2 and mouse_y < UiHeight() * 0.02

            if is_over_text then
                previous_highlight[title] = 1
            end

            if not previous_highlight[title] then
                previous_highlight[title] = 0
            end

            if previous_highlight[title] > 0 then
                UiTextShadow(0.933333333, 0.68627451, 0, 1, 0, previous_highlight[title])
                previous_highlight[title] = previous_highlight[title] - dt
            end

            if previous_highlight[title] < 0 then
                previous_highlight[title] = 0
            end

            UiColor(0.933333333, 0.68627451, 0)
        else
            UiColor(0.407843137, 0.407843137, 0.407843137)
        end

        UiText(title)
    UiPop()

    UiPush()
        local description_font_size = UiHeight() * 0.035
        UiFont("MOD/assets/fonts/FiraSans-Regular.ttf", description_font_size)
        UiColor(0.407843137, 0.407843137, 0.407843137)
        UiTranslate(0.2112 * UiWidth(), 0)

        UiText(description)
    UiPop()

    local entry_height = UiHeight() * 0.0665
    UiTranslate(0, entry_height)
end

function doMenu(dt)
    UiMakeInteractive()

    local t = GetTime()
    local frame = (t * 24) % 110;  -- 24 frames per second, 109 frames
    
    local logo_path = "MOD/assets/image_sequence/logo/"
    local logo_name = "logo_" .. string.format("%04d", frame + 1) .. ".png"
    logo_path = logo_path .. logo_name
    
    local background_path = "MOD/assets/image/menu_background.png"

    local logo_width, logo_height = UiGetImageSize(logo_path)
    local background_width, background_height = UiGetImageSize(background_path)
    local background_fraction_width = background_width / UiWidth()
    local background_fraction_height = background_height / UiHeight()
    local background_size = math.min(background_fraction_width, background_fraction_height)

    UiPush()
        UiPush()
            UiTranslate(UiCenter(), UiMiddle())
            UiAlign("center middle")
            UiImageBox(background_path, background_width / background_size, background_height / background_size)
        UiPop()
        UiPush()

        UiPush()
            UiTranslate(UiWidth() * 0.0888, UiHeight() * 0.5828)
            doMenuEntry(dt, "New game", "Start a new single player game.")
            doMenuEntry(dt, "Load game", "Load a previously saved game.")
            doMenuEntry(dt, "Find servers", "Search for online multiplayer servers.", true)
            doMenuEntry(dt, "Create server", "Host an online multiplayer server for others to join.", true)
            doMenuEntry(dt, "Options", "Change game settings, configure controls.", true)
            doMenuEntry(dt, "Quit", "Quit playing Half-Life.")
        UiPop()
        --[[
        UiPush()
            UiTranslate(UiWidth() * 0.1, UiHeight() * 0.1)
            UiImageBox(logo_path, UiWidth(), logo_height / logo_width * UiWidth())
        UiPop()
        ]]
    UiPop()
end

function draw(dt)
    
    if doIntro() then
        return
    end

    doMenu(dt)
end
