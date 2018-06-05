-- TÖVE Editor.
-- (C) 2018 Bernhard Liebl, MIT license.

local ColorWheel = {}
ColorWheel.__index = ColorWheel

local pi2 = 2 * math.pi

local function barycentric(px, py, x1, y1, x2, y2, x3, y3)
	local dx = px - x3
	local dy = py - y3
	local denom = (y2 - y3) * (x1 - x3) + (x3 - x2) * (y1 - y3)
	local u = ((y2 - y3) * dx + (x3 - x2) * dy) / denom
	local v = ((y3 - y1) * dx + (x1 - x3) * dy) / denom
	local w = 1.0 - u - v
	return u, v, w
end

local function hsv(h, s, v)
	-- https://love2d.org/wiki/HSV_color
    if s <= 0 then return v,v,v end
    if h < 0 then
    	h = h + 1
    end
    h, s, v = h*6, s, v
    local c = v*s
    local x = (1-math.abs((h%2)-1))*c
    local m,r,g,b = (v-c), 0,0,0
    if h < 1     then r,g,b = c,x,0
    elseif h < 2 then r,g,b = x,c,0
    elseif h < 3 then r,g,b = 0,c,x
    elseif h < 4 then r,g,b = 0,x,c
    elseif h < 5 then r,g,b = x,0,c
    else              r,g,b = c,0,x
    end return (r+m),(g+m),(b+m)
end

function ColorWheel:computeTriangleVertices()
	local phi = self.currentHue
	local phi1 = phi + 2 * math.pi * (1 / 3)
	local phi2 = phi + 2 * math.pi * (2 / 3)

	local radius = self.radius

	return {
		{ radius * math.cos(phi), radius * math.sin(phi), hsv(phi / pi2, 1, 1) },
		{ radius * math.cos(phi1), radius * math.sin(phi1), 1, 1, 1 },
		{ radius * math.cos(phi2), radius * math.sin(phi2), 0, 0, 0 }
	}
end

function ColorWheel:colorChanged()
	local triangleSpot = self.triangleSpot
	local triangleVertices = self.triangleVertices

	local color = {}
	for j = 1, 3 do
		local v = 0
		for i = 1, 3 do
			v = v + triangleSpot[i] * triangleVertices[i][3 + j - 1]
		end
		color[j] = v
	end

	self.callback(unpack(color))
end

function ColorWheel:updateSL(mx, my, mode)
	local triangleVertices = self.triangleVertices
	local ax, ay = unpack(triangleVertices[1])
	local bx, by = unpack(triangleVertices[2])
	local cx, cy = unpack(triangleVertices[3])
	local u, v, w = barycentric(mx - self.x, my - self.y, ax, ay, bx, by, cx, cy)
	if mode ~= "check" then
		u = math.max(u, 0)
		v = math.max(v, 0)
		w = math.max(w, 0)
		local s = u + v + w
		u = u / s
		v = v / s
		w = w / s
	end
	if u < 0 or v < 0 or w < 0 then
		return false
	end
	self.triangleSpot = {u, v, w}
	self:colorChanged()
	return true
end

function ColorWheel:updateHue(mx, my)
	local dx = mx - self.x
	local dy = my - self.y
	self.currentHue = math.atan2(dy, dx)
	self.triangleVertices = self:computeTriangleVertices()
	self.triangle:setVertices(self.triangleVertices)
	self:colorChanged()
end

function ColorWheel:draw()
	if not self.visible then
		return
	end

	local x, y = self.x, self.y
	local halfsize = self.halfsize
	local scale = self.scale

	love.graphics.setColor(1, 1, 1, 1)
	love.graphics.setBlendMode("alpha", "premultiplied")
	love.graphics.draw(self.canvas, x - halfsize, y - halfsize, 0, scale, scale)
	love.graphics.setBlendMode("alpha")
	love.graphics.draw(self.triangle, x, y)

	local circle = self.circle

	local r = self.radius + self.thickness / 2
	local currentHue = self.currentHue
	circle:draw(x + math.cos(currentHue) * r, y + math.sin(currentHue) * r)

	local triangleVertices = self.triangleVertices
	local ax, ay = unpack(triangleVertices[1])
	local bx, by = unpack(triangleVertices[2])
	local cx, cy = unpack(triangleVertices[3])
	local u, v, w = unpack(self.triangleSpot)
	circle:draw(x + ax * u + bx * v + cx * w, y + ay * u + by * v + cy * w)
end

function ColorWheel:click(mx, my)
	if not self.visible then
		return
	end

	local dx = mx - self.x
	local dy = my - self.y
	local d = math.sqrt(dx * dx + dy * dy)
	local r = self.radius
	if d >= r and d <= r + self.thickness then
		self:updateHue(mx, my)
		return function(mx, my)
			return self:updateHue(mx, my)
		end
	else
		if self:updateSL(mx, my, "check") then
			return function(mx, my)
				return self:updateSL(mx, my)
			end
		end
		return nil
	end
end

return function(callback)
	local oversample = 1

	local code = [[
	#define M_PI 3.1415926535897932384626433832795

	uniform float radius;
	uniform float thickness;

	// https://github.com/hughsk/glsl-hsv2rgb
	vec3 hsv2rgb(vec3 c) {
		vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
		vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
		return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
	}

	vec4 effect(vec4 _, Image texture, vec2 texture_coords, vec2 screen_coords){
		float d = length(texture_coords);

		float phi = atan(texture_coords.y, texture_coords.x) / (M_PI * 2);
		vec3 color = hsv2rgb(vec3(phi, 1, 1));

		float alpha = smoothstep(radius - 1, radius + 1, d) *
			(1.0f - smoothstep(radius + thickness - 1, radius + thickness + 1, d));

		return vec4(color, alpha);
	}
	]]

	local radius = 60
	local thickness = 15
	local halfsize = radius + thickness
	local size = halfsize * 2

	local shader = love.graphics.newShader(code)
	shader:send("radius", radius)
	shader:send("thickness", thickness)

	local attributes = {{"VertexPosition", "float", 2}, {"VertexTexCoord", "float", 2}}

	local pixelsize = size * oversample

	local vertices = {
		{ 0, 0, -halfsize, -halfsize },
		{ pixelsize, 0, halfsize, -halfsize },
		{ pixelsize, pixelsize, halfsize, halfsize },
		{ 0, pixelsize, -halfsize, halfsize }
	}

	local mesh = love.graphics.newMesh(attributes, vertices, "fan")

	canvas = love.graphics.newCanvas(pixelsize, pixelsize)
	love.graphics.setCanvas(canvas)
    love.graphics.clear()
	love.graphics.setShader(shader)
	love.graphics.draw(mesh)
	love.graphics.setShader()
	love.graphics.setCanvas()

	canvas:setFilter("linear", "linear")

	local circle = tove.newGraphics()
	circle:drawCircle(0, 0, thickness / 3)
	circle:setLineColor(0, 0, 0)
	circle:setLineWidth(0.9)
	circle:stroke()

	local x, y = love.graphics.getWidth() - halfsize, halfsize

	local cw = setmetatable({
		x = x,
		y = y,
		visible = false,

		radius = radius,
		thickness = thickness,

		halfsize = halfsize,
		scale = 1 / oversample,

		canvas = canvas,
		circle = circle,

		currentHue = 0,
		triangleVertices = nil,
		triangleSpot = {1, 0, 0},
		triangle = nil,

		callback = callback
	}, ColorWheel)

	do
		local attributes = {{"VertexPosition", "float", 2}, {"VertexColor", "float", 3}}
		cw.triangleVertices = cw:computeTriangleVertices()
		cw.triangle = love.graphics.newMesh(attributes, cw.triangleVertices, "fan", "dynamic")
	end

	cw:colorChanged()

	return cw
end
