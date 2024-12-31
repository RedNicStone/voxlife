#include "script/toolanimation.lua"

local function rndVec(length)
	local v = VecNormalize(Vec(math.random(-100,100), math.random(-100,100), math.random(-100,100)))
	return VecScale(v, length)	
end

local function rnd(mi, ma)
	return math.random(1000)/1000*(ma-mi) + mi
end

local function Reflect(I, N)
	return VecSub(I, VecScale(N, 2 * VecDot(N, I)))
end

local function DrawTauLaser(laserPoints)
	local laserSprite = LoadSprite("gfx/laser.png")

	local col = Vec(1, 0.75, 0.3)

	for i = 1, #laserPoints-1 do
		local origin = laserPoints[i]
		local hitPoint = laserPoints[i+1]
		local length = VecLength(VecSub(hitPoint, origin))
		local t = Transform(VecLerp(origin, hitPoint, 0.5))
		local xAxis = VecNormalize(VecSub(hitPoint, origin))
		local zAxis = VecNormalize(VecSub(origin, GetCameraTransform().pos))
		t.rot = QuatAlignXZ(xAxis, zAxis)
		DrawSprite(laserSprite, t, length, 0.05+math.random()*0.01, col[1] * 4, col[2] * 4, col[3] * 4, 1, true, true)
		DrawSprite(laserSprite, t, length, 0.5, col[1], col[2], col[3], 1, true, true)
		PointLight(origin, col[1], col[2], col[3], 3)
		PointLight(hitPoint, col[1], col[2], col[3], 3)
	end
end

tau = {}

function tau:init()
	-- code
	RegisterTool("vl_tau", "Tau Cannon", "MOD/tools/tau/prefab.xml", 4)
	SetBool("game.tool.vl_tau.enabled", true)
	self.angle = 0
	self.angVel = 0
	self.coolDown = 0
	self.smoke = 0
	self.laser = 0
	self.laserPoints = {}
	self.shootSnd = {}
	self.shootSnd2 = {}
	self.playSecondarySound = 0
	for i=1, 1 do
		self.shootSnd[i] = LoadSound("MOD/tools/tau/shot"..i..".ogg")
	end
	for i=1, 3 do
		self.shootSnd2[i] = LoadSound("MOD/tools/tau/electro"..i..".ogg")
	end
	self.oldPipePos = Vec()
	self.particleTimer = 0
	self.shape = 1
	self.toolAnimator = ToolAnimator()
	SetInt("game.tool.vl_tau.ammo", 1800)
end

function tau:tick(dt)
	if self.playSecondarySound > 0 then
		self.playSecondarySound = self.playSecondarySound - dt
		if self.playSecondarySound <= 0 then
			PlaySound(self.shootSnd2[math.random(1,#self.shootSnd2)], self.laserPoints[1], 0.25)
		end
	end

	if GetString("game.player.tool") == "vl_tau" then
		local ct = GetCameraTransform()

		if GetBool("game.player.canusetool") and InputDown("usetool") and GetInt("game.tool.vl_tau.ammo") > 0 then
			if self.coolDown < 0 then
				local distance = 1000

				local forwardPos = TransformToParentPoint(ct, Vec(0, 0, -distance))
				local rayPos = ct.pos
				local rayDir = VecSub(forwardPos, rayPos)
				rayDir = VecNormalize(rayDir)
				local trn = GetToolLocationWorldTransform("muzzle_right")
				trn = TransformToLocalTransform(self.shapeOriginalTransforms[1], trn)
				trn = TransformToParentTransform(GetShapeLocalTransform(self.shapes[1]), trn)

				self.laserPoints = {}
				self.laserPoints[1] = trn.pos

				for bounceI = 1,10 do
					local hit, hitDistance, hitNormal = QueryRaycast(rayPos, rayDir, distance)
					local hitPos = VecAdd(rayPos, VecScale(rayDir, 1000))
					if hit and hitDistance < distance then
						hitPos = VecAdd(rayPos, VecScale(rayDir, hitDistance))
					end
					-- Paint(hitPos, 0.6, "explosion")

					if hit and hitDistance < distance then
						self.laserPoints[1 + bounceI] = hitPos
						local cosRefl = VecDot(VecNormalize(rayDir), VecNormalize(hitNormal))
						rayDir = Reflect(rayDir, hitNormal)
						rayPos = VecAdd(hitPos, VecScale(rayDir, 0.01))

						for particleI = 1,6 do
							ParticleReset()
							ParticleType('plain')
							ParticleCollide(1, 1, 'constant', 0.1)
							ParticleGravity(-10)
							ParticleDrag(0.025)
							ParticleSticky(0.1, 0.35)
							ParticleEmissive(1)
							ParticleStretch(100)
							ParticleTile(4)
							local pVel = VecScale(VecAdd(hitNormal, VecAdd(rayDir, rndVec(0.75))), 2)
							ParticleAlpha(20)
							ParticleColor(1, 0.8, 0.4, 1, 0.6, 0.1)
							ParticleRadius(0.03)
							SpawnParticle(rayPos, pVel, 2)
						end

						if cosRefl < -0.5 then
							break
						end
					else
						self.laserPoints[1 + bounceI] = hitPos
						break
					end
				end
				
				self.firePos = self.laserPoints[1]
				local hitPos = self.laserPoints[#self.laserPoints-0]
				local shootPos = self.laserPoints[#self.laserPoints-1]
				local shootDir = VecNormalize(VecSub(hitPos, shootPos))
				shootPos = VecAdd(shootPos, VecScale(shootDir, 0.01))
				
				Shoot(shootPos, shootDir, "gun", 1)
				PlaySound(self.shootSnd[math.random(1,#self.shootSnd)], self.laserPoints[1], 0.25)

				self.playSecondarySound = 0.1

				self.smoke = math.min(1.0, self.smoke + 0.2)
				self.coolDown = 0.25
				self.particleTimer = -0.5
				SetInt("game.tool.vl_tau.ammo", GetInt("game.tool.vl_tau.ammo")-1)

				self.laser = 0.05
			end
		end

		local p = self.firePos
		if self.smoke > 0 then
			if self.particleTimer < 0.0 then
				self.particleTimer = dt + (1.0-self.smoke)*0.05
				local vel = VecScale(VecSub(p, self.oldPipePos), 0.5/ dt)
				vel = VecAdd(vel, Vec(0, rnd(0, 2), 0))
				ParticleReset()
				ParticleType("smoke")
				ParticleRadius(0.08, 0.15)
				ParticleAlpha(self.smoke*0.9, 0)
				ParticleDrag(1.0)
				ParticleTile(0)
				SpawnParticle(p, VecAdd(vel, rndVec(0.1)), self.particleTimer*50)
			end
		end

		if self.laser > 0 then
			DrawTauLaser(self.laserPoints)
		end


		self.particleTimer = self.particleTimer - dt*550
		self.oldPipePos = p
	
		self.coolDown = self.coolDown - dt
		self.angle = self.angle + self.angVel*dt
		
		--Move tool a bit to the right and recoil
		local t = Transform()
		local recoil = math.max(0, self.coolDown) * 0.05

		local b = GetToolBody()
		local voxSize = 0.3
		local attach = Transform(Vec(0, 0, 0), Quat())
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
		-- attach.pos[1] = 0.3*recoil
		-- attach.pos[2] = attach.pos[2]+2.0*recoil
		-- attach.rot = QuatEuler(300*recoil, 0, 0)
		-- handsTransform.pos[3] = 0.3*recoil
		-- handsTransform.rot = QuatEuler(300*recoil, 0, 0)
		-- righHandTransform = TransformToParentTransform(handsTransform, righHandTransform)
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
	self.laser = math.max(0.0, self.laser - dt)
end

function tau:draw(dt)
	if GetString("game.player.tool") == "vl_tau" then
		cross = UiPixelToWorld(UiCenter(), UiMiddle())
	end
end
