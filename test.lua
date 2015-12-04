l = require "lastar"

local blockmap = {
	0, 1, 0, 0, 0,
	0, 0, 0, 1, 0,
	1, 1, 1, 1, 0,
	0, 0, 0, 1, 0,
	0, 1, 0, 0, 0,
}

local from = {x=1, y=1}
local to = {x=1, y=5}

local function neighbor(fx, fy, tx, ty)
	local idx = tx + (ty - 1) * w
	if blockmap[idx] ~= 0 then
		return
	end

	local dis = (tx - fx) * (ty - fy) > 0 and 1 or 1.4
	return true, dis
end

local map = l.new(5, 5, neighbor)

local path, cost = map:path(from.x, from.y, to.x, to.y)

--[[
table.foreach(path, function(i, v)
	print(v[1], v[2])
end)
print(cost)
--]]
