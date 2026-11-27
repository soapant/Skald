local sk = require("skapi")

local function add(display_name, on_action, icon)
    sk.add_to_cc({
        display_name = display_name,
        on_action = on_action,
        type = "Window Management",
        icon = icon,
    })
end

local function close_by_title()
    local needle = sk.popup({ placeholder = "Close windows with matching title" })
    if needle and needle ~= "" then
        sk.close_windows_by_title(needle)
    end
end

local function close_by_id()
    local needle = sk.popup({ placeholder = "Close windows with matching id" })
    if needle and needle ~= "" then
        sk.close_windows_by_id(needle)
    end
end

add("Move Active Window Right", sk.move_active_window_right, "/arrow-right.png")
add("Move Active Window Left", sk.move_active_window_left, "/arrow-left.png")
add("Move Active Window Up", sk.move_active_window_up, "/arrow-up.png")
add("Move Active Window Down", sk.move_active_window_down, "/arrow-down.png")

add("Focus Window Right", sk.focus_window_right, "/arrow-right.png")
add("Focus Window Left", sk.focus_window_left, "/arrow-left.png")
add("Focus Window Up", sk.focus_window_up, "/arrow-up.png")
add("Focus Window Down", sk.focus_window_down, "/arrow-down.png")

add("Close Active Window", sk.close_active_window, "/x.png")
add("Toggle Fullscreen", sk.toggle_fullscreen_active, "/maximize-2.png")
add("Toggle Minimized", sk.toggle_minimized_active, "/minimize-2.png")
add("Toggle Maximize", sk.toggle_maximize_active, "/expand.png")
add("Toggle Always On Top", sk.toggle_always_on_top_active, "/pin.png")
add("Raise Active Window", sk.raise_active_window, "/chevrons-up.png")
add("Lower Active Window", sk.lower_active_window, "/chevrons-down.png")
add("Center Active Window", sk.center_active_window, "/crosshair.png")

local snaps = {
    { "Snap Left", 0, "/panel-left.png" },
    { "Snap Right", 1, "/panel-right.png" },
    { "Snap Top", 2, "/panel-top.png" },
    { "Snap Bottom", 3, "/panel-bottom.png" },
    { "Snap Top Left", 4, "/arrow-up-left.png" },
    { "Snap Top Right", 5, "/arrow-up-right.png" },
    { "Snap Bottom Left", 6, "/arrow-down-left.png" },
    { "Snap Bottom Right", 7, "/arrow-down-right.png" },
}
for _, entry in ipairs(snaps) do
    add(entry[1], function() sk.snap_active_window(entry[2]) end, entry[3])
end

add("Move Window To Next Monitor", function() sk.move_active_window_to_monitor(true) end, "/move-horizontal.png")
add("Move Window To Previous Monitor", function() sk.move_active_window_to_monitor(false) end, "/move-horizontal.png")

for i = 1, 9 do
    add("Switch To Workspace " .. i, function() sk.switch_to_workspace(i) end, "/ws" .. i .. ".png")
    add("Move Window To Workspace " .. i, function() sk.move_active_window_to_workspace(i) end, "/mws" .. i .. ".png")
end

local opacities = { { "Full", 100 }, { "80", 80 }, { "60", 60 }, { "40", 40 } }
for _, entry in ipairs(opacities) do
    add("Set Opacity " .. entry[1], function() sk.set_active_window_opacity(entry[2]) end, "/percent.png")
end

add("Close Windows By Title", close_by_title, "/x.png")
add("Close Windows By Id", close_by_id, "/box.png")
