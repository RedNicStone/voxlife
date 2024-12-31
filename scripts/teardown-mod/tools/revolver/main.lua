#include "script/toolanimation.lua"

local function rndVec(length)
	local v = VecNormalize(Vec(math.random(-100,100), math.random(-100,100), math.random(-100,100)))
	return VecScale(v, length)	
end

local function rnd(mi, ma)
	return math.random(1000)/1000*(ma-mi) + mi
end

revolver = {}

function revolver:init()
	RegisterTool("vl_revolver", "Revolver", "MOD/tools/revolver/prefab.xml", 2)
    SetBool("game.tool.vl_revolver.enabled", true)
	self.angle = 0
	self.angVel = 0
	self.coolDown = 0
	self.smoke = 0
	self.shootSnd = {}
	for i=1, 1 do
		self.shootSnd[i] = LoadSound("MOD/tools/revolver/shot"..i..".ogg")
	end
	self.oldPipePos = Vec()
	self.particleTimer = 0
	self.shape = 1
	self.toolAnimator = ToolAnimator()
	SetInt("game.tool.vl_revolver.ammo", 1800)
end

function revolver:tick(dt)
	if GetString("game.player.tool") == "vl_revolver" then
		local ct = GetCameraTransform()

		if GetBool("game.player.canusetool") and InputDown("usetool") and GetInt("game.tool.vl_revolver.ammo") > 0 then
			if self.coolDown < 0 then
				self.firePos = (GetToolLocationWorldTransform("muzzle_right") or GetCameraTransform()).pos

				local forwardPos = TransformToParentPoint(ct, Vec(0, 0, -1000))
				local direction = VecSub(forwardPos, ct.pos)
				local distance = VecLength(direction)
				direction = VecNormalize(direction)

				local hit, hitDistance = QueryRaycast(ct.pos, direction, distance)
				if hit then
					forwardPos = TransformToParentPoint(ct, Vec(0, 0, -hitDistance))
					distance = hitDistance
				end
				local aimDir = VecNormalize(VecSub(forwardPos, self.firePos))

				Shoot(self.firePos, aimDir, "gun", 1)
				--Light, particles and sound
				local lp = TransformToParentPoint(ct, Vec(0.25, -0.25, -2.0))
				PointLight(lp, 1, 0.7, 0.5, 3)
				PlaySound(self.shootSnd[math.random(1,#self.shootSnd)], self.firePos, 0.25)

				self.smoke = math.min(1.0, self.smoke + 0.2)
				self.coolDown = 0.75
				self.particleTimer = -0.5
				SetInt("game.tool.vl_revolver.ammo", GetInt("game.tool.vl_revolver.ammo")-1)
			end
		end

		local p = self.firePos
		if self.smoke > 0 then
			if self.particleTimer < 0.0 then
				self.particleTimer = dt + (1.0-self.smoke)*0.05
				local vel = VecScale(VecSub(p, self.oldPipePos), 0.5/ dt)
				vel = VecAdd(vel, Vec(0, rnd(0, 2), 0))
				ParticleType("smoke")
				ParticleRadius(0.08, 0.15)
				ParticleAlpha(self.smoke*0.9, 0)
				ParticleDrag(1.0)
				SpawnParticle(p, VecAdd(vel, rndVec(0.1)), self.particleTimer*50)
			end
		end
		self.particleTimer = self.particleTimer - dt*550
		self.oldPipePos = p
	
		self.coolDown = self.coolDown - dt
		self.angle = self.angle + self.angVel*dt
		
		--Move tool a bit to the right and recoil
		local t = Transform()
		local recoil = math.max(0, self.coolDown) * 0.2

		local b = GetToolBody()
		local voxSize = 0.3
		local attach = Transform(Vec(0.5*voxSize, -1.5*voxSize, 0), Quat())
		local handsTransform = Transform()
		local leftHandTransform = self.toolAnimator.leftHand.transform
		local righHandTransform = self.toolAnimator.rightHand.transform

		if self.body ~= b then
			self.body = b
			-- Barrel is the second shape in vox file. Remember original position in attachment frame
			self.shapes = GetBodyShapes(b)
			self.shapeOriginalTransforms = {}
			for i = 1, #self.shapes do
				self.shapeOriginalTransforms[i] = TransformToLocalTransform(attach, GetShapeLocalTransform(self.shapes[i]))
			end
		end
		attach.pos[3] = 0.3*recoil
		attach.rot = QuatEuler(300*recoil, 0, 0)
		handsTransform.pos[3] = 0.1*recoil
		handsTransform.rot = QuatEuler(300*recoil, 0, 0)
		righHandTransform = TransformToParentTransform(handsTransform, righHandTransform)
		for i = 1, #self.shapes do
			t = TransformToParentTransform(attach, self.shapeOriginalTransforms[i])
			SetShapeLocalTransform(self.shapes[i], t)
		end
		self.toolAnimator.leftHand.transform = leftHandTransform
		self.toolAnimator.rightHand.transform = righHandTransform
		self.toolAnimator.armPitchScale = 0.4
		self.toolAnimator.toolPitchScale = 0.4
		if getEyeHeight() < 0.9 then
			self.toolAnimator.armPitchScale = 0.0
			self.toolAnimator.toolPitchScale = 0.4
		end
		tickToolAnimator(self.toolAnimator, dt)
	end
	self.smoke = math.max(0.0, self.smoke - dt)
end

function revolver:draw()
	if GetString("game.player.tool") == "vl_revolver" then
		cross = UiPixelToWorld(UiCenter(), UiMiddle())
	end
end
