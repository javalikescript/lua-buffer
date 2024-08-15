local lu = require('luaunit')

local bufferLib = require('buffer')

function Test_new_size()
  local b = bufferLib.new(256)
  lu.assertEquals(bufferLib.len(b), 256)
  lu.assertNil(bufferLib.new())
  lu.assertNil(bufferLib.new(0))
  lu.assertNil(bufferLib.new(''))
end

function Test_new_string()
  local s = 'Hello world!'
  local b = bufferLib.new(s)
  lu.assertEquals(bufferLib.len(b), string.len(s))
  lu.assertEquals(bufferLib.sub(b), s)
  lu.assertNil(bufferLib.new(''))
end

function Test_new_userdata()
  local s = 'Hello world!'
  local ud = bufferLib.new(s)
  local b = bufferLib.new(ud)
  lu.assertEquals(bufferLib.len(b), string.len(s))
  lu.assertEquals(bufferLib.sub(b), s)
end

function Test_byte()
  local s = 'Hello world!'
  local b = bufferLib.new(s)
  lu.assertEquals(bufferLib.byte(b, 1), string.byte(s, 1))
  lu.assertEquals({bufferLib.byte(b, 1, 3)}, {string.byte(s, 1, 3)})
  lu.assertEquals(bufferLib.byte(b), string.byte(s))
  lu.assertEquals(table.pack(bufferLib.byte(b)), table.pack(string.byte(s)))
  lu.assertEquals(table.pack(bufferLib.byte(b, 3, -1)), table.pack(string.byte(s, 3, -1)))
  -- TODO corrected interval values
end

function Test_byteset()
  local s = 'Hello world!'
  local b = bufferLib.new(s)
  lu.assertEquals(bufferLib.sub(b), s)
  bufferLib.byteset(b, 7)
  lu.assertEquals(bufferLib.sub(b), s)
  bufferLib.byteset(b, 3, 32, 32, 32)
  lu.assertEquals(bufferLib.sub(b), 'He    world!')
  bufferLib.byteset(b, 7, bufferLib.byte(b, 7) - 32)
  lu.assertEquals(bufferLib.sub(b), 'He    World!')
end

function Test_byteset_mupltiple()
  local b = bufferLib.new(3)
  bufferLib.byteset(b, 1, 65, 66, 67)
  lu.assertEquals(bufferLib.sub(b), 'ABC')
end

function Test_memset()
  local b = bufferLib.new(3)
  bufferLib.memset(b, 65)
  lu.assertEquals(bufferLib.sub(b), 'AAA')
  b = bufferLib.new('---')
  bufferLib.memset(b, 32)
  lu.assertEquals(bufferLib.sub(b), '   ')
end

local function checkMemCopy(b, c)
  bufferLib.memcpy(b, c, 7)
  lu.assertEquals(bufferLib.sub(b), 'Hello earth!')
  bufferLib.memcpy(b, 'World', 7)
  lu.assertEquals(bufferLib.sub(b), 'Hello World!')
  bufferLib.memcpy(b, c, 8, 2, 4)
  lu.assertEquals(bufferLib.sub(b), 'Hello Wartd!')
  bufferLib.memcpy(b, c, -6)
  lu.assertEquals(bufferLib.sub(b), 'Hello earth!')
  bufferLib.memcpy(b, 'Hi   ')
  lu.assertEquals(bufferLib.sub(b), 'Hi    earth!')
end

function Test_memcpy()
  local b = bufferLib.new('Hello world!')
  local c = bufferLib.new('earth')
  checkMemCopy(b, c)
end

function Test_memcpy_string()
  local b = bufferLib.new('Hello world!')
  checkMemCopy(b, 'earth')
end

function Test_memcpy_light()
  local bb = bufferLib.new('Hello world!')
  local l = bufferLib.len(bb)
  local b = bufferLib.lighten(bb)
  local c = bufferLib.new('earth')
  bufferLib.memcpy(b, c, 7)
  lu.assertEquals(bufferLib.sub(bb), 'Hello earth!')
  bufferLib.memcpy(b, 'World', 7)
  lu.assertEquals(bufferLib.sub(bb), 'Hello World!')
  bufferLib.memcpy(b, c, 8, 2, 4)
  lu.assertEquals(bufferLib.sub(bb), 'Hello Wartd!')
  bufferLib.memcpy(b, c, -6)
  lu.assertEquals(bufferLib.sub(bb), 'Hello Wartd!')
  bufferLib.memcpy(b, 'Hi   ')
  lu.assertEquals(bufferLib.sub(bb), 'Hi    Wartd!')
end

local function checkSub(b)
  lu.assertEquals(bufferLib.sub(b, 1, 5), 'Hello')
  lu.assertEquals(bufferLib.sub(b, 7, 11), 'world')
  lu.assertEquals(bufferLib.sub(b, 2, 1), '')
end

local function checkSubLight(b)
  checkSub(b)
  lu.assertEquals(bufferLib.sub(b), '')
  lu.assertEquals(bufferLib.sub(b, 7), '')
  lu.assertEquals(bufferLib.sub(b, 7, -2), '')
end

function Test_sub()
  local s = 'Hello world!'
  local b = bufferLib.new(s)
  checkSub(b)
  lu.assertEquals(bufferLib.sub(b), s)
  lu.assertEquals(bufferLib.sub(b, 7), 'world!')
  lu.assertEquals(bufferLib.sub(b, 7, -2), 'world')
end

function Test_sub_light()
  local s = 'Hello world!'
  local b = bufferLib.new(s)
  local l = bufferLib.lighten(b)
  checkSubLight(l)
end

function Test_sub_pointer()
  local b = bufferLib.new('Hi')
  local l = bufferLib.lighten(b)
  local p = bufferLib.topointer(b)
  local lp = bufferLib.frompointer(p)
  lu.assertEquals(type(l), 'userdata')
  lu.assertEquals(type(p), 'string')
  lu.assertEquals(type(lp), 'userdata')
  lu.assertEquals(l, lp)
end

os.exit(lu.LuaUnit.run())
