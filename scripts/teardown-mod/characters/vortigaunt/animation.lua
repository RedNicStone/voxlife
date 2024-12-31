
------------------------------------------------------------------------
model = {}
model.animator = 0

------------------------------------------------------------------------
function init()
	model.animator = FindAnimator()
end


function tick(dt)
	PlayAnimationLoop(model.animator, "Standing Idle 02")
	-- MakeRagdoll(model.animator)
end
