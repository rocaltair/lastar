l = require "lastar"

--[[
-- blocks :
-- 	bitarray, 1 for block, 0 for avaliable
-- 	each elem is 32bit integer
--]]
local mapdata = {width=1000, height=1000, blocks={0x0,0xffffffff,0xffffffff,0xffffffff}}
local from = {x=1, y=1}
local to = {x=1000, y=1000}

local map = l.new(mapdata.blocks, mapdata.width, mapdata.height)

local path, cost = map:path(from.x, from.y, to.x, to.y)

--[[
table.foreach(path, function(i, v)
	print(v.x, v.y)
end)
print(cost)
--]]
