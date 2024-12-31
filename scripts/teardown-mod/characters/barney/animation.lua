
------------------------------------------------------------------------
model = {}
model.animator = 0


------------------------------------------------------------------------
function init()
	model.animator = FindAnimator()
	animationName = "idle_"..(math.floor(math.random(6)))
	
end


function tick(dt)
	PlayAnimationLoop(model.animator, animationName)
end
