local lu = require('luaunit')

local bufferLib = require('buffer')

function test_new_size()
  local b = bufferLib.new(256);
  lu.assertEquals(bufferLib.len(b), 256)
end

function test_new_string()
  local s = 'Hello world!'
  local b = bufferLib.new(s);
  lu.assertEquals(bufferLib.len(b), string.len(s))
  lu.assertEquals(bufferLib.tostring(b), s)
end

function test_new_userdata()
  local s = 'Hello world!'
  local ud = bufferLib.new(s);
  local b = bufferLib.new(ud);
  lu.assertEquals(bufferLib.len(b), string.len(s))
  lu.assertEquals(bufferLib.tostring(b), s)
end

function test_byte()
  local s = 'Hello world!'
  local b = bufferLib.new(s);
  lu.assertEquals(bufferLib.byte(b, 1), string.byte(s, 1))
  lu.assertEquals({bufferLib.byte(b, 1, 3)}, {string.byte(s, 1, 3)})
  lu.assertEquals(bufferLib.byte(b), string.byte(s))
  -- TODO corrected interval values
end

function test_byteset()
  local s = 'Hello world!'
  local b = bufferLib.new(s);
  bufferLib.byteset(b, 7, bufferLib.byte(b, 7) - 32);
  lu.assertEquals(bufferLib.tostring(b), 'Hello World!')
end

function test_byteset_mupltiple()
  local b = bufferLib.new(3);
  bufferLib.byteset(b, 1, 65, 66, 67);
  lu.assertEquals(bufferLib.tostring(b), 'ABC')
end

function test_memset()
  local b = bufferLib.new(3);
  bufferLib.memset(b, 65);
  lu.assertEquals(bufferLib.tostring(b), 'AAA')
  b = bufferLib.new('---');
  bufferLib.memset(b, 32);
  lu.assertEquals(bufferLib.tostring(b), '   ')
end

function test_memcpy()
  local b = bufferLib.new('Hello world!');
  local c = bufferLib.new('earth');
  bufferLib.memcpy(b, 7, c);
  lu.assertEquals(bufferLib.tostring(b), 'Hello earth!')
  bufferLib.memcpy(b, 7, 'World');
  lu.assertEquals(bufferLib.tostring(b), 'Hello World!')
  bufferLib.memcpy(b, 8, c, 2, 4);
  lu.assertEquals(bufferLib.tostring(b), 'Hello Wartd!')
  bufferLib.memcpy(b, -6, c);
  lu.assertEquals(bufferLib.tostring(b), 'Hello earth!')
  -- TODO corrected interval values
end

function test_memcpy_light()
  local bb = bufferLib.new('Hello world!');
  local l = bufferLib.len(bb)
  local b = bufferLib.lighten(bb)
  local c = bufferLib.new('earth');
  bufferLib.memcpy(b, 7, c);
  lu.assertEquals(bufferLib.tostring(bb), 'Hello earth!')
  bufferLib.memcpy(b, 7, 'World');
  lu.assertEquals(bufferLib.tostring(bb), 'Hello World!')
  bufferLib.memcpy(b, 8, c, 2, 4);
  lu.assertEquals(bufferLib.tostring(bb), 'Hello Wartd!')
  bufferLib.memcpy(b, -6, c);
  lu.assertEquals(bufferLib.tostring(bb), 'Hello Wartd!')
end

function test_sub()
  local s = 'Hello world!'
  local b = bufferLib.new(s);
  lu.assertEquals(bufferLib.sub(b), s)
  lu.assertEquals(bufferLib.sub(b, 1, 4), 'Hell')
  lu.assertEquals(bufferLib.sub(b, 7), 'world!')
  lu.assertEquals(bufferLib.sub(b, 7, -2), 'world')
  lu.assertEquals(bufferLib.sub(b, 2, 1), '')
end

function test_sub_light()
  local s = 'Hello world!'
  local bb = bufferLib.new(s);
  local b = bufferLib.lighten(bb)
  lu.assertEquals(bufferLib.sub(b), '')
  lu.assertEquals(bufferLib.sub(b, 1, 4), 'Hell')
  lu.assertEquals(bufferLib.sub(b, 7), '')
  lu.assertEquals(bufferLib.sub(b, 7, -2), '')
  lu.assertEquals(bufferLib.sub(b, 2, 1), '')
end

os.exit(lu.LuaUnit.run())
