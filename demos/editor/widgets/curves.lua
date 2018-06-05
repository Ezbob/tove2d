-- TÖVE Editor.
-- (C) 2018 Bernhard Liebl, MIT license.

local PointsWidget = {}
PointsWidget.__index = PointsWidget

local function lengthSqr(dx, dy)
	return dx * dx + dy * dy
end

local function isControlPointVisible(selected, traj, i, j, k)
	if selected ~= nil and selected.path == i and selected.traj == j then
		local s = selected.pt
		local distance = math.abs(s - k)
		distance = math.min(distance, traj.npts - distance)

		return distance <= 2
	end
end

function PointsWidget:allpoints(f)
	local graphics = self.object.graphics

	for i = 1, graphics.npaths do
		local path = graphics.paths[i]
		for j = 1, path.ntrajs do
			local traj = path.trajs[j]

			for k = 1, traj.npts, 3 do
				local pts = traj.pts
				local pt = pts[k]
				local dragfunc = f(pts, i, j, k, pt.x, pt.y)
				if dragfunc ~= nil then
					return dragfunc
				end
			end
		end
	end

	return nil
end

function PointsWidget:allcontrolpoints(f)
	local graphics = self.object.graphics
	local selected = self.selected
	local dragging = self.dragging

	for i = 1, graphics.npaths do
		local path = graphics.paths[i]
		for j = 1, path.ntrajs do
			local traj = path.trajs[j]

			for k = 1, traj.npts, 3 do
				local pts = traj.pts

				local isActive = false
				if dragging ~= nil and dragging.path == i and dragging.traj == j and dragging.pt0 == k then
					isActive = true
				end

				if isActive or isControlPointVisible(selected, traj, i, j, k - 1) then
					local pt1 = pts[k - 1]
					local dragfunc = f(traj, pts, i, j, k, k - 1, pt1.x, pt1.y)
					if dragfunc ~= nil then
						return dragfunc
					end
				end

				if isActive or isControlPointVisible(selected, traj, i, j, k + 1) then
					local pt2 = pts[k + 1]
					local dragfunc = f(traj, pts, i, j, k, k + 1, pt2.x, pt2.y)
					if dragfunc ~= nil then
						return dragfunc
					end
				end
			end
		end
	end

	return nil
end

function PointsWidget:oncurve(lx, ly)
	local graphics = self.object.graphics
	for i = 1, graphics.npaths do
		local path = graphics.paths[i]
		for j = 1, path.ntrajs do
			local traj = path.trajs[i]
			local t = traj:closest(lx, ly, 2 + path:getLineWidth())
			if t >= 0 then
				return i, j, t
			end
		end
	end
	return -1, -1, -1
end

local function polar(x, y, ox, oy)
	local dx = x - ox
	local dy = y - oy
	return math.atan2(dy, dx), math.sqrt(dx * dx + dy * dy)
end

local function mirrorControlPoint(qx, qy, px, py, p0x, p0y, rx, ry)
	local oldphi, oldmag = polar(qx, qy, p0x, p0y)
	local newphi, newmag = polar(px, py, p0x, p0y)

	local cp1phi, cp1mag = polar(rx, ry, p0x, p0y)

	cp1phi = cp1phi + (newphi - oldphi)

	return p0x + math.cos(cp1phi) * cp1mag, p0y + math.sin(cp1phi) * cp1mag
end

function PointsWidget:updateOverlayLine()
	local overlayLine = self.overlayLine
	local handleColor = self.handles.color
	overlayLine:set(self.object.scaledgraphics)
	for i = 1, overlayLine.npaths do
		local p = overlayLine.paths[i]
		p:setFillColor()
		p:setLineColor(unpack(handleColor))
		p:setLineWidth(1)
	end
end

function PointsWidget:draw()
	local transform = self.object.transform
	local handles = self.handles

	love.graphics.push("transform")
	love.graphics.applyTransform(transform.drawtransform)
	self.overlayLine:draw()
	love.graphics.pop()

	local tfm = transform.mousetransform

	self:allcontrolpoints(function(traj, pts, i, j, k0, k, lx, ly)
		local x, y = tfm:transformPoint(lx, ly)
		local kx, ky = tfm:transformPoint(pts[k0].x, pts[k0].y)
		love.graphics.line(kx, ky, x, y)
		handles.selected:draw(x, y, 0, 0.75, 0.75)
	end)

	local selected = self.selected

	self:allpoints(function(pts, i, j, k, lx, ly)
		local handle = handles.normal
		if selected ~= nil and selected.path == i and selected.traj == j and selected.pt == k then
			handle = handles.selected
		end
		local x, y = tfm:transformPoint(lx, ly)
		handle:draw(x, y)
	end)
end

function PointsWidget:createDragPointFunc()
	local transform = self.object.transform
	local graphics = self.object.graphics
	local dragging = self.dragging

	return function(gx, gy)
		self.object:changePoints(function()
			local mx, my = transform:inverseTransformPoint(gx, gy)

			local traj = graphics.paths[dragging.path].trajs[dragging.traj]
			local p = traj.pts[dragging.pt]

			local qx, qy = p.x, p.y

			p.x = mx
			p.y = my

			if (dragging.pt - 1) % 3 == 0 then
				-- if this is not a control point, also drag adjacent control points.
				local dx = mx - qx
				local dy = my - qy

				local cp0 = traj.pts[dragging.pt - 1]
				cp0.x = cp0.x + dx
				cp0.y = cp0.y + dy

				local cp1 = traj.pts[dragging.pt + 1]
				cp1.x = cp1.x + dx
				cp1.y = cp1.y + dy
			else
				-- if control point, mirror movement on other control point
				local p0 = traj.pts[dragging.pt0]
				local p = traj.pts[dragging.pt]
				local cp1 = traj.pts[dragging.pt0 - (dragging.pt - dragging.pt0)]

				cp1.x, cp1.y = mirrorControlPoint(qx, qy, mx, my, p0.x, p0.y, cp1.x, cp1.y)
			end
		end)

		self:updateOverlayLine()
	end
end

function PointsWidget:createMouldCurveFunc(i, j, t)
	local transform = self.object.transform
	local traj = self.object.graphics.paths[i].trajs[j]

	return function(gx, gy)
		local mx, my = transform:inverseTransformPoint(gx, gy)

		local ci = 1 + math.floor(t)
		local c1 = traj.curves[ci]

		local cp1x, cp1y = c1.cp1x, c1.cp1y
		local cp2x, cp2y = c1.cp2x, c1.cp2y

		traj:mould(t, mx, my)

		local prev = traj.curves[ci - 1]
		prev.cp2x, prev.cp2y = mirrorControlPoint(
			cp1x, cp1y, c1.cp1x, c1.cp1y, c1.x0, c1.y0, prev.cp2x, prev.cp2y)

		local next = traj.curves[ci + 1]
		next.cp1x, next.cp1y = mirrorControlPoint(
			cp2x, cp2y, c1.cp2x, c1.cp2y, c1.x, c1.y, next.cp1x, next.cp1y)

		self.object:refresh()
		self:updateOverlayLine()
	end
end

function PointsWidget:mousedown(gx, gy)
	local transform = self.object.transform
	local lx, ly = transform:inverseTransformPoint(gx, gy)

	local clickRadiusSqr = self.handles.clickRadiusSqr

	local dragfunc = self:allpoints(function(pts, i, j, k, px, py)
		if lengthSqr(lx - px, ly - py) < clickRadiusSqr then
			self.selected = {path = i, traj = j, pt = k}
			self.dragging = self.selected
			return self:createDragPointFunc()
		end
	end)
	if dragfunc ~= nil then
		return dragfunc
	end

	dragfunc = self:allcontrolpoints(function(traj, pts, i, j, k0, k, px, py)
		if lengthSqr(lx - px, ly - py) < clickRadiusSqr then
			self.dragging = {path = i, traj = j, pt = k, pt0 = k0}
			return self:createDragPointFunc()
		end
	end)
	if dragfunc ~= nil then
		return dragfunc
	end

	-- click on curve?
	local i, j, t = self:oncurve(lx, ly)
	if t >= 0 then
		return self:createMouldCurveFunc(i, j, t)
	end

	return nil
end

function PointsWidget:click(gx, gy)
	local transform = self.object.transform
	local lx, ly = transform:inverseTransformPoint(gx, gy)

	local clickRadiusSqr = self.handles.clickRadiusSqr
	if self:allpoints(function(pts, i, j, k, px, py)
		if lengthSqr(lx - px, ly - py) < clickRadiusSqr then
			return true
		end
	end) then
		return
	end

	local i, j, t = self:oncurve(lx, ly)
	if t >= 0 then
		local traj = self.object.graphics.paths[i].trajs[j]
		local k = traj:insertCurveAt(t)
		self.selected = {path = i, traj = j, pt = k}
		self.object:refresh()
		self:updateOverlayLine()
	end
end

function PointsWidget:mousereleased()
	self.dragging = nil
end

function PointsWidget:keypressed(key)
	if key == "backspace" then
		local selected = self.selected
		if selected ~= nil then
			if (selected.pt - 1) % 3 == 0 then
				self.object:changePoints(function()
					local traj = self.object.graphics.paths[selected.path].trajs[selected.traj]
					traj:removeCurve((selected.pt - 1) / 3)
				end)
				self:updateOverlayLine()
			end
		end
	end
end

return function(handles, object)
	local overlayLine = tove.newGraphics()
	overlayLine:setDisplay("mesh")
	overlayLine:setUsage("points", "dynamic")

	local pw = setmetatable({
		handles = handles,
		object = object,
		selected = nil,
		dragging = nil,
		overlayLine = overlayLine}, PointsWidget)

	pw:updateOverlayLine()

	return pw
end
