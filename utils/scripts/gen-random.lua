
--[[
-- Copyright (C) Jianyong Chen
--]]

if #arg ~= 2 then
    error("usage: lua gen-random.lua <#files> <#nums/file>")
end

local maxnum1 = 100000
local maxnum2 = 100
local nfiles = tonumber(arg[1])
local nnums = tonumber(arg[2]);

for i = 1, nfiles do
    local file = io.open("file-" .. i, "w")

    for j = 1, nnums do
        local first = math.floor(math.random() * maxnum1 + 1)
        local second = math.floor(math.random() * maxnum2 + 1)

        file:write(string.format("%d\t%d\n", first, second))
    end

    file:close()
end
