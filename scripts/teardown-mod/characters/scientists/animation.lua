
------------------------------------------------------------------------
model = {}
model.animator = 0



------------------------------------------------------------------------
function init()
	model.animator = FindAnimator()
	animationName = "idle_"..(math.floor(math.random(6)))
end

-- t = 0
-- prevT = 0

function tick(dt)
	-- PlayAnimationLoop(model.animator, "emote_1")
	PlayAnimationLoop(model.animator, animationName)
	-- t = t + dt
	-- if math.floor(t) > prevT then
	-- 	local i = prevT + 1
	-- 	prevT = math.floor(t)
	-- 	if i <= #list then
	-- 		local name = list[i]
	-- 		DebugPrint(name)
	-- 	end
	-- end
end
